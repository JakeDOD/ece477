#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define FLAG_PORT "-p"
#define FLAG_INPUT "-f"
#define FLAG_OUTPUT "-o"

#define BUFF_SIZE 100

// Parses the program arguments for the port and output file name
void parseArgs(int argc, char* argv[], char** port, char** inputName, char** outputName)
{
	// Parse the command line arguments
	int i = 0;
	for (i = 0; i < argc; i++) {

		// Compare the input arguments against our known flags
		if ((strcmp(FLAG_PORT, argv[i]) == 0) && (argc > ++i)) {
			*port = argv[i];
		} else if ((strcmp(FLAG_INPUT, argv[i]) == 0) && (argc > ++i)) {
			*inputName = argv[i];
		} else if ((strcmp(FLAG_OUTPUT, argv[i]) == 0) && (argc > ++i)) {
			*outputName = argv[i];
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
	cfsetospeed(&tty, B9600);
	cfsetispeed(&tty, B9600);

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
		return -3;
	}

	/* First flush any data not read or trasmitted */
	tcflush(fd, TCIOFLUSH);

	return fd;
}

int childMain(int fd, char* outputName)
{
	FILE* outputFile = stdout;
	FILE* inputFile = NULL;
	char* inBuff[BUFF_SIZE];
	int retVal;
	int num1, num2, num3, num4, num5 = 0;
	char peek;

	// If we have an output file, open that file for writing
	if (outputName != NULL) {
		outputFile = fopen(outputName, "w");
		if (outputFile == NULL) {
			perror("There was an error opening the output file");
			return errno;
		}
	}

	inputFile = fdopen(fd, "r");
	if (inputFile == NULL) {
		perror("fdopen()");
		return errno;
	}

	// Loop forever
	while (1) {
		retVal = fscanf(inputFile, "%d,%d,%d,%d,%d", &num1, &num2, &num3, &num4, &num5);
		if (retVal == EOF) {
			return errno;
		}

		switch (retVal) {
			case 1:
				fprintf(outputFile, "%d\n", num1);
				break;
			case 2:
				fprintf(outputFile, "%d,%d\n", num1, num2);
				break;
			case 3:
				fprintf(outputFile, "%d,%d,%d\n", num1, num2, num3);
				break;
			case 4:
				fprintf(outputFile, "%d,%d,%d,%d\n", num1, num2, num3, num4);
				break;
			case 5:
				fprintf(outputFile, "%d,%d,%d,%d,%d\n", num1, num2, num3, num4, num5);
				break;
		}
	}

	return 0;
}

int parentMain(pid_t childPid, int fd, char* inputName)
{
	char buff[BUFF_SIZE];
	int inputFd, exitValue, retVal;
	int sleepTime = 0;

	// First open the input file
	inputFd = open(inputName, O_RDONLY);
	if (inputFd < 0) {
		perror("There was an error opening the input file");
		fprintf(stderr, "\tInput File: %s\n", inputName);
		exitValue = -1;
	} else {

		// Read from the file and write its contents to the serial port
		while (1) {
			retVal = read(inputFd, buff, BUFF_SIZE);
			if (retVal < 0) {
				fprintf(stderr, "%s\n", inputName);
				perror("There was an error reading the input file");
				exitValue = -2;
				break;
			} else if (retVal == 0) {
				// We've reached the end of the file
				exitValue = 0;
				break;
			}

			// No errors, let's write to the serial port
			if (write(fd, buff, strlen(buff)) < strlen(buff)) {
				perror("Error writing to the serial port");
				exitValue = -3;
				break;
			}

			sleepTime += 1000 * strlen(buff);
		}
	}

	// Close the serial port fd
	if (close(fd) != 0) {
		perror("Error closing the serial port");
		exitValue = -4;
	}

	usleep(sleepTime);

	if (kill(childPid, SIGTERM) != 0) {
		perror("There was an error while calling kill()");
		return -5;
	}

	// Wait for the child process to return
	waitpid(childPid, &retVal, 0);

	if (retVal < 0) {
		fprintf(stderr, "The child process returned an error\n");
		exitValue = -6;
	}

	return exitValue;
}

int main(int argc, char* argv[])
{
	static char defaultPort[] = "/dev/ttyUSB0";
	static char defaultInput[] = "default.csv";
	char* port, *inputName, *outputName = NULL;
	int fd;
	pid_t pid;

	parseArgs(argc, argv, &port, &inputName, &outputName);

	// If we didn't find our input arguments, set them to the default
	if (port == NULL) {
		port = defaultPort;
	}

	if (inputName == NULL) {
		inputName = defaultInput;
	}

	// Set up the serial port
	fd = setupPort(port);
	if (fd < 0) {
		return -1;
	}

	// Here we will split into two programs
	pid = fork();
	if (pid < 0) {
		perror("Error calling fork()");

		// Close the serial port
		if (close(fd) < 0) {
			perror("Error calling close()");
			return -2;
		}

		return -3;
	} else if (pid == 0) {
		// We are the child process
		return childMain(fd, outputName);
	} else {
		// We are the parent process
		return parentMain(pid, fd, inputName);
	}

	return 0;
}
