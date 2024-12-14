#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

// #define USE_SHELL_REBOOT
#define INPUT_PATH "/dev/input/"
#define MAX_DEVICES 256
#define KEY KEY_POWER
#define REBOOT_TARGET "recovery"
#define TIMEOUT 2000

#ifndef USE_SHELL_REBOOT
#include <sys/system_properties.h>
#endif

// poll_fd_to_filename[0] is always NULL since it's inotify_fd
char *poll_fd_to_filename[MAX_DEVICES];
int already_fd_numbers = 1;
int inotify_fd = -1;
int dry_run = 0;
int use_syslog = 0;
struct pollfd poll_fds[MAX_DEVICES];

/*
void log_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (use_syslog)
    vsyslog(LOG_ERR, fmt, args);
  else
    vfprintf(stderr, fmt, args);
  va_end(args);
}*/

#define log_error(fmt, ...)                                                    \
  do {                                                                         \
    if (use_syslog) {                                                          \
      syslog(LOG_ERR, fmt, __VA_ARGS__);                                       \
    } else {                                                                   \
      fprintf(stderr, fmt "\n", __VA_ARGS__);                                  \
    }                                                                          \
  } while (0)
#define perror_log(msg) log_error(msg ": %s", strerror(errno))

void reboot(int signal) {
  if (dry_run) {
    log_error("reboot triggered", NULL); // Weird workaround...
  } else {
#ifdef USE_SHELL_REBOOT
    system("reboot " REBOOT_TARGET);
#else
    __system_property_set("sys.powerctl", "reboot," REBOOT_TARGET);
#endif
  }
}

int try_open_device(const char *device_name) {
  if (strncmp(device_name, "event", 5) != 0 ||
      strlen(device_name) + sizeof(INPUT_PATH) > PATH_MAX)
    return -1;
  char fullpath[PATH_MAX] = INPUT_PATH;

  strlcpy((char *)&(fullpath) + sizeof(INPUT_PATH) - 1, device_name,
          strlen(device_name) + 1);
  int new_fd = open(fullpath, O_RDONLY);
  if (new_fd < 0)
    log_error("Cannot open %s: %s", fullpath, strerror(errno));
  return new_fd;
}

int find_available_slot() {
  int available;
  for (int i = 0; i < already_fd_numbers; i++)
    if (poll_fds[i].fd == -1)
      return i;
  already_fd_numbers++;
  return already_fd_numbers - 1;
}

int handle_inotify() {
  uint8_t event_buf[sizeof(struct inotify_event) + NAME_MAX + 1];
  uint8_t *cur_ptr;
  ssize_t size;
  while (1) {
    size = read(inotify_fd, &event_buf, sizeof(event_buf));
    if (size == -1 && errno != EAGAIN) {
      log_error("Cannot handle inotify: %s", strerror(errno));
      return 0;
    } else if (size <= 0) {
      return 1;
    }
    cur_ptr = (uint8_t *)&event_buf;
    while (cur_ptr < event_buf + size) {
      struct inotify_event *event = (struct inotify_event *)cur_ptr;
      cur_ptr += sizeof(struct inotify_event) + event->len;
      if (event->mask & IN_ISDIR || !event->len)
        continue;
      if (already_fd_numbers <= MAX_DEVICES && event->mask & IN_CREATE) {
        int new_fd;
        if ((new_fd = try_open_device(event->name)) < 0)
          continue;
        int available_slot = find_available_slot();
        poll_fds[available_slot].fd = new_fd;
        poll_fds[available_slot].events = POLLIN;
        poll_fd_to_filename[available_slot] = strdup(event->name);
      } else if (event->mask & IN_DELETE &&
                 strncmp(event->name, "event", 5) == 0) {
        int found = 0;
        for (int i = 1; i < already_fd_numbers; i++) {
          if (strcmp(poll_fd_to_filename[i], event->name) == 0) {
            found = 1;
            if (close(poll_fds[i].fd) == -1)
              log_error("Cannot close fd %d (%s): %s", poll_fds[i].fd,
                        event->name, strerror(errno));
            poll_fds[i].fd = -1;
            poll_fds[i].events = 0;
            poll_fds[i].revents = 0;
            if (i + 1 == already_fd_numbers)
              already_fd_numbers -= 1;
            break;
          }
        }
        if (!found)
          log_error(
              "Warning: No corresponding opened file was found for " INPUT_PATH
              "%s",
              event->name);
      }
    }
  }
  return 1;
}

int handle_input(int fd) {
  struct input_event event;
  if (read(fd, &event, sizeof(event)) != sizeof(event)) {
    log_error("Cannot read event from %d: %s", fd, strerror(errno));
    return 0;
  }
  if (event.type == EV_KEY && event.code == KEY) {
    struct itimerval it_val;
    it_val.it_value.tv_sec = 0;
    it_val.it_value.tv_usec = 0;
    it_val.it_interval = it_val.it_value;
    if (event.value == 1) {
      it_val.it_value.tv_sec = TIMEOUT / 1000;
      it_val.it_value.tv_usec = (TIMEOUT * 1000) % 1000000;
      it_val.it_interval = it_val.it_value;
    }
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1)
      perror_log("Cannot setitimer");
  }
  return 1;
}

int main(int argc, char *argv[]) {
  char stderr_link[11];
  stderr_link[readlink("/proc/self/fd/2", stderr_link, 10)] = '\0';
  if (strcmp(stderr_link, "/dev/null") == 0) {
    use_syslog = 1;
    openlog("lpfrd", LOG_CONS, LOG_USER);
  }

  int ret = 1;
  if (argc == 2 && strcmp(argv[1], "-d") == 0) {
    dry_run = 1;
  } else if (argc != 1) {
    log_error("Usage: %s [-d]", argv[0]);
    return 1;
  }

  if (signal(SIGALRM, reboot) == SIG_ERR) {
    perror_log("Cannot set SIGALRM handler");
    return 1;
  }
  inotify_fd = inotify_init1(IN_NONBLOCK);
  if (inotify_fd < 0 ||
      inotify_add_watch(inotify_fd, INPUT_PATH, IN_DELETE | IN_CREATE) < 0) {
    perror_log("Cannot initialize inotify");
    goto fail;
  }
  poll_fds[0].fd = inotify_fd;
  poll_fds[0].events = POLLIN;
  poll_fd_to_filename[0] = NULL;

  DIR *dir = opendir(INPUT_PATH);
  struct dirent *dirent;
  if (!dir) {
    perror_log("Cannot opendir " INPUT_PATH);
    goto fail;
  }
  while ((dirent = readdir(dir)) != NULL && already_fd_numbers <= MAX_DEVICES) {
    int new_fd = try_open_device(dirent->d_name);
    if (new_fd) {
      poll_fds[already_fd_numbers].fd = new_fd;
      poll_fds[already_fd_numbers].events = POLLIN;
      poll_fd_to_filename[already_fd_numbers] = strdup(dirent->d_name);
      already_fd_numbers++;
    }
  }
  closedir(dir);

  while (poll(poll_fds, already_fd_numbers, -1) != -1) {
    // Handle inotify in last
    for (int i = 1; i < already_fd_numbers; i++)
      if (poll_fds[i].revents != 0 && poll_fds[i].revents & POLLIN)
        handle_input(poll_fds[i].fd);
    if (poll_fds[0].revents != 0 && poll_fds[0].revents & POLLIN)
      handle_inotify();
  }
  if (errno != EINTR)
    perror_log("Polling stopped");
  else
    ret = 0;
fail:
  for (int i = 0; i < already_fd_numbers; i++)
    if (poll_fd_to_filename[i])
      free(poll_fd_to_filename[i]);
  if (use_syslog)
    closelog();
  return ret;
}
