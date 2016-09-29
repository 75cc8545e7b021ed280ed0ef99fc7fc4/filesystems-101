/* See cmsg(3) to learn more. */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>


static int prepare_file_to_pass(void)
{
	char template[] = "pass-fd-XXXXXX";
	int fd = mkstemp(template);
	if (fd < 0)
		err(1, "mkstemp() failed");

	unlink(template);

	char hello[] = "hello";
	write(fd, hello, strlen(hello));

	lseek(fd, 0, SEEK_SET);

	return fd;
}

int main()
{
	int fd = prepare_file_to_pass();

	int sock[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock) != 0)
		err(1, "socketpair() failed");

	pid_t child = fork();
	if (child < 0)
		err(1, "fork() failed");

	if (child > 0) {
		/* we are the parent; pass an fd to the child */

		/* we cannot send ancillary data only, there needs to be some normal data */
		char dummy[] = {0};
		struct iovec iov = {.iov_base = dummy, .iov_len = sizeof(dummy)};

		union {
			char data[CMSG_SPACE(sizeof(int))];
			struct cmsghdr align;
		} u;

		struct msghdr msg = {
			.msg_name = NULL,
			.msg_namelen = 0,
			.msg_iov = &iov,
			.msg_iovlen = 1,
			.msg_control = &u.data,
			.msg_controllen = sizeof(u.data),
			.msg_flags = 0
		};

		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

		if (sendmsg(sock[0], &msg, 0) != sizeof(dummy))
			err(1, "sendmsg() failed");

		int status;
		waitpid(child, &status, 0);

		exit(0);
	} else {
		/* we are the child; receive an fd, and use it */

		int fd_child;

		/* we cannot receive ancillary data only, there needs to be some normal data */
		char dummy[] = {0};
		struct iovec iov = {.iov_base = dummy, .iov_len = sizeof(dummy)};

		union {
			char data[CMSG_SPACE(sizeof(int))];
			struct cmsghdr align;
		} u;

		struct msghdr msg = {
			.msg_name = NULL,
			.msg_namelen = 0,
			.msg_iov = &iov,
			.msg_iovlen = 1,
			.msg_control = &u.data,
			.msg_controllen = sizeof(u.data),
			.msg_flags = 0
		};

		if (recvmsg(sock[1], &msg, 0) != sizeof(dummy))
			err(1, "recvmsg() failed");

		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
			errx(1, "received something other than SCM_RIGHTS: level = %i, type = %i", cmsg->cmsg_level, cmsg->cmsg_type);

		memcpy(&fd_child, CMSG_DATA(cmsg), sizeof(int));

		fprintf(stderr, "fd = %i, fd_child = %i\n", fd, fd_child);

		char buf[128];

		memset(buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf) - 1);
		fprintf(stderr, "reading from %i produced: '%s'\n", fd, buf);

		/* fds @fd and @fd_child are really duplicates of each other, and reading from @fd
		   changes the position of @fd_child, so we need to rewind it */
		lseek(fd_child, 0, SEEK_SET);

		memset(buf, 0, sizeof(buf));
		read(fd_child, buf, sizeof(buf) - 1);
		fprintf(stderr, "reading from %i produced: '%s'\n", fd_child, buf);

		_exit(0);
	}
}
