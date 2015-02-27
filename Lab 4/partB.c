#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define FLAG_PORT "-p"
#define FLAG_CLIENT "-c"
#define FLAG_SERVER "-s"

#define FALSE 0
#define TRUE 1

#define BUFF_SIZE 1

// Parses the program arguments for the port and output file name
void parseArgs(int argc, char* argv[], char** port, int* isServer)
{
	*isServer = -1;

	// Parse the command line arguments
	int i = 0;
	for (i = 0; i < argc; i++) {

		// Compare the input arguments against our known flags and ignores other flags
		if ((strcmp(FLAG_PORT, argv[i]) == 0) && (argc > ++i)) {
			*port = argv[i];
		} else if ((strcmp(FLAG_CLIENT, argv[i]) == 0) && (*isServer == -1)) {
			*isServer = FALSE;
		} else if ((strcmp(FLAG_SERVER, argv[i]) == 0) && (*isServer == -1)) {
			*isServer = TRUE;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Unknown argument: %s\n", argv[i]);
		}
	}
}

// Sets up the serial port and returns a file descriptor for the port
int setupPort(char* portName)
{
	int fd;
	struct termios tty;

	/* Open the serial port */
	fd = open(portName, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) {
		perror("There was an error opening the serial port");
		fprintf(stderr, "%s\n", portName);
		return fd;
	}

	/* Get the serial port attribute info */
	if (tcgetattr(fd, &tty) != 0) {
		perror("Error getting serial port info");
		return -2;
	}

	/* Set the read and write speed to baud 115200 */
	if (cfsetospeed(&tty, B9600) || cfsetispeed(&tty, B9600)) {
		perror("cfsetspeed");
		return -3;
	}

	/* Set the data size to 8 bits */
	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	
	/* Disable break processing */
	tty.c_iflag &= ~IGNBRK;
	tty.c_lflag = 0;

	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 10;

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	tty.c_cflag |= (CLOCAL | CREAD);

	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag |= 0;

	// Set two stop bits
	tty.c_cflag |= CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	/* Set the terminal attributes */
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		perror("There was an error setting the serial port info");
		return -4;
	}

	/* First flush any data not read or trasmitted */
	tcflush(fd, TCIOFLUSH);

	return fd;
}

int clientMain(int fd)
{
	char buff[BUFF_SIZE];
	int retVal;

	// Sit and read input from the command line
	while (1) {
		retVal = read(fd, buff, BUFF_SIZE);
		if (retVal < 0) {
			perror("There was an error reading from the serial port");
			return -2;
		} else if (retVal == 0) {
			// We've reached the end of the file
			return 0;
		}

		// No errors, let's write to the serial port
		if (fwrite(buff, sizeof(buff[0]), BUFF_SIZE, stdout) < BUFF_SIZE) {
			perror("Error writing to the stdout");
			return -3;
		}
	}
}

int serverMain(int fd)
{
	char buff[BUFF_SIZE];
	int retVal;

	// Sit and read input from the command line
	while (1) {
		retVal = fread(buff, sizeof(buff[0]), BUFF_SIZE, stdin);
		if (retVal < 0) {
			perror("There was an error reading the stdin");
			return -2;
		} else if (retVal == 0) {
			// We've reached the end of the file
			return 0;
		}

		// No errors, let's write to the serial port
		if (write(fd, buff, BUFF_SIZE) < BUFF_SIZE) {
			perror("Error writing to the serial port");
			return -3;
		}
	}
}

int main(int argc, char *argv[])
{
	static char defaultPort[] = "/dev/ttyUSB0";
	char* port = NULL;
	int isServer;
	int fd;

	// Parse the command line arguments
	parseArgs(argc, argv, &port, &isServer);

	// If we didn't find our input arguments, set them to the default
	if (port == NULL) {
		port = defaultPort;
	}

	// Set up the serial port
	fd = setupPort(port);
	if (fd < 0) {
		return -1;
	}

	if (isServer) {
		return serverMain(fd);
	} else {
		return clientMain(fd);
	}

	return 0;
}