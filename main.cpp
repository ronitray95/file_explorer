#include <sys/types.h>
#include <dirent.h>
#include <termios.h>
#include <unistd.h>
#include <sys/errno.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>

#include <iostream>
#include <string>
#include <cstring>
#include <stack>
#include <vector>

using namespace std;

const int KEY_ESCAPE = 0x001b;
const int KEY_ENTER = 0x000a;
const int KEY_UP = 0x0105;
const int KEY_DOWN = 0x0106;
const int KEY_LEFT = 0x0107;
const int KEY_RIGHT = 0x0108;

char ROOT_PATH[1024];
struct termios old_tio, new_tio;
struct winsize WindowSize;
stack<string> prevBackDir;
stack<string> prevFwdDir;
vector<dirent *> listOfDirs;
vector<string> typeFileOrDir;
unsigned int xC = 1; //x co-ord for cursor
unsigned int yC = 1; //y co-ord for cursor
unsigned int term_row;
unsigned int term_col;
int currList = 0;

int getch()
{
	int c = 0;
	tcgetattr(0, &old_tio);
	memcpy(&new_tio, &old_tio, sizeof(new_tio));
	new_tio.c_lflag &= ~(ICANON | ECHO);
	new_tio.c_cc[VMIN] = 1;
	new_tio.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &new_tio);
	c = getchar();
	tcsetattr(0, TCSANOW, &old_tio);
	return c;
}

void clearScreen()
{
	printf("\033[H\033[J");
}

int kbhit()
{
	int c = 0;
	tcgetattr(0, &old_tio);
	memcpy(&new_tio, &old_tio, sizeof(new_tio));
	new_tio.c_lflag &= ~(ICANON | ECHO);
	new_tio.c_cc[VMIN] = 0;
	new_tio.c_cc[VTIME] = 1;
	tcsetattr(0, TCSANOW, &new_tio);
	c = getchar();
	tcsetattr(0, TCSANOW, &old_tio);
	if (c != -1)
		ungetc(c, stdin);
	return ((c != -1) ? 1 : 0);
}

int kbesc()
{
	int c;
	if (!kbhit())
		return KEY_ESCAPE;
	c = getch();
	if (c == '[')
	{
		switch (getch())
		{
		case 'A':
			c = KEY_UP;
			break;
		case 'B':
			c = KEY_DOWN;
			break;
		case 'C':
			c = KEY_LEFT;
			break;
		case 'D':
			c = KEY_RIGHT;
			break;
		default:
			c = 0;
			break;
		}
	}
	else
		c = 0;
	if (c == 0)
		while (kbhit())
			getch();
	return c;
}

int kbget()
{
	int c;
	c = getch();
	return (c == KEY_ESCAPE) ? kbesc() : c;
}

void moveForward(int x)
{
	printf("\033[%dC", x);
}
void moveBackward(int x)
{
	printf("\033[%dD", x);
}

void performOp(int x)
{
	switch (x)
	{
	case 1: //key right
		break;
	case 2: //key left
		break;
	case 3:
		printf("\033[%dA", 1);
		break;
	case 4:
		printf("\033[%dB", 1);
		break;
	case 5:
		break;
	case 6:
		break;
	case 7:
		break;
	case 8:
		break;
	}
}

void navigate()
{
	int c;
	while (true)
	{
		c = kbget();
		if (c == KEY_ENTER || c == KEY_ESCAPE)
		{
			break;
		}
		else if (c == KEY_RIGHT)
		{
			performOp(1);
		}
		else if (c == KEY_LEFT)
		{
			performOp(2);
		}
		else if (c == KEY_UP)
		{
			performOp(3);
		}
		else if (c == KEY_DOWN)
		{
			performOp(4);
		}
		else if (c == 'H' || c == 'h') //go to home

		{
			performOp(5);
		}
		else if (c == 127) //backspace = up one level
		{
			performOp(6);
		}
		else if (c == 10) //enter - execute file or enter dir
		{
			performOp(7);
		}
		else if (c == ':') //command mode
		{
			performOp(8);
		}
		else if (c == 'q' || c == 'Q') //quit application
		{
			clearScreen();
			break;
		}
		else
		{
			putchar(c);
		}
	}
}

void listDirCurrentPath(char *path)
{
	struct dirent *de;
	struct stat fileDetails;
	DIR *dr = opendir(path);
	listOfDirs.clear();
	typeFileOrDir.clear();
	clearScreen();
	if (dr == NULL)
	{
		printf("Could not open current directory");
		return;
	}
	printf("Permissions\tOwner\tGroup\tSize(B)\tLast Modified\tName\n");
	while ((de = readdir(dr)) != NULL)
	{
		listOfDirs.push_back(de);
		int detailsStatus = stat(de->d_name, &fileDetails);
		mode_t perms = fileDetails.st_mode;
		string permissions = "";
		permissions = permissions + ((S_ISDIR(perms)) ? "d" : "-");
		if (permissions == "d")
			typeFileOrDir.push_back("dir");
		else
			typeFileOrDir.push_back("file");
		permissions = permissions + ((perms & S_IRUSR) ? "r" : "-");
		permissions = permissions + ((perms & S_IWUSR) ? "w" : "-");
		permissions = permissions + ((perms & S_IXUSR) ? "x" : "-");
		permissions = permissions + ((perms & S_IRGRP) ? "r" : "-");
		permissions = permissions + ((perms & S_IWGRP) ? "w" : "-");
		permissions = permissions + ((perms & S_IXGRP) ? "x" : "-");
		permissions = permissions + ((perms & S_IROTH) ? "r" : "-");
		permissions = permissions + ((perms & S_IWOTH) ? "w" : "-");
		permissions = permissions + ((perms & S_IXOTH) ? "x" : "-");

		struct passwd *pw = getpwuid(fileDetails.st_uid);
		struct group *gr = getgrgid(fileDetails.st_gid);
		struct tm *tm = localtime(&fileDetails.st_mtime);
		string owner = (pw == NULL ? "Undefined" : pw->pw_name);
		string grp = (gr == NULL ? "Undefined" : gr->gr_name);
		char modTime[256];
		strftime(modTime, sizeof(modTime), nl_langinfo(D_T_FMT), tm);

		printf("%s \t %-8.8s\t%-8.8s\t%9jd\t%s\t%s\n", permissions.c_str(), pw->pw_name, gr->gr_name, (intmax_t)fileDetails.st_size, modTime, de->d_name);
	}
	currList = listOfDirs.size();
	closedir(dr);
}

int main()
{
	getcwd(ROOT_PATH, 1024);
	listDirCurrentPath(ROOT_PATH);
	navigate();
	return 0;
}