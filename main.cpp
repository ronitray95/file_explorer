#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <termios.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <stack>
#include <vector>

#include "commandList.h"
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define PLATFORM_NAME "windows"
#elif __APPLE__
#define PLATFORM_NAME "mac"
#elif __linux__
#define PLATFORM_NAME "unix"
#elif __unix__ // all unices not caught above
#define PLATFORM_NAME "unix"
#else
# #define PLATFORM_NAME "other"
#endif

using namespace std;

const int KEY_ESCAPE = 0x001b;
const int KEY_ENTER = 0x000a;
const int KEY_UP = 0x0105;
const int KEY_DOWN = 0x0106;
const int KEY_LEFT = 0x0107;
const int KEY_RIGHT = 0x0108;
const int COMMAND_MODE = 2;
const int NORMAL_MODE = 1;
const int SCROLL_CONSTANT = 5;
const string TYPE_FILE = "file";
const string TYPE_DIRECTORY = "dir";
char ROOT_PATH[4096];

int mode = NORMAL_MODE;
int CURRENT_ROW;
char currentPath[4096];
char inputBuffer[4096];
int ibc = 0;
struct termios old_tio, new_tio;
struct winsize WindowSize;
vector<string> prevBackDir;
vector<string> prevFwdDir;
vector<dirent *> listOfDirs;
vector<string> typeFileOrDir;
unsigned int xC = 1; //x co-ord for cursor
unsigned int yC = 1; //y co-ord for cursor
unsigned int term_row;
unsigned int term_col;
int currList = 0;

void listDirCurrentPath(char const *);
void navigateCommandMode(vector<string>);
void displayAtIndex(int);

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
	if (c == EOF)
		clearerr(stdin);
	//printf("Returning %d\n",c);
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
	if (c != -1 && c != EOF)
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
			c = KEY_RIGHT;
			break;
		case 'D':
			c = KEY_LEFT;
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
	//printf("Esc is %d got back is %d",KEY_ESCAPE,c);
	return (c == KEY_ESCAPE) ? kbesc() : c;
}

void performOp(int x)
{
	string fName, p;
	dirent *dir;
	switch (x)
	{
	case 1: //key right
		if (prevFwdDir.size() == 0)
			break;
		prevBackDir.push_back(string(currentPath));
		//strcpy(currentPath, prevFwdDir.back().c_str());
		p = prevFwdDir.back();
		prevFwdDir.pop_back();
		listDirCurrentPath(p.c_str());
		break;
	case 2: //key left
		if (prevBackDir.size() == 0)
			break;
		prevFwdDir.push_back(string(currentPath));
		//strcpy(currentPath, prevBackDir.back().c_str());
		p = prevBackDir.back();
		prevBackDir.pop_back();
		listDirCurrentPath(p.c_str());
		break;
	case 3: //key UP
		//printf("\033M");
		//break;
		CURRENT_ROW = max(CURRENT_ROW - 1, 1);
		if (yC > 1)
		{
			yC--;
			if (CURRENT_ROW != 1)
				printf("\033[%dA", 1); //move cursor up
			else
			{
				printf("\033M");  //scroll up
				printf("\33[2K"); //clear line
				displayAtIndex(yC - 1);
				cout << "\033[" << 1 << ";" << 1 << "H";
			}
		}
		break;
	case 4: //key down
		CURRENT_ROW = min(CURRENT_ROW + 1, (int)WindowSize.ws_row);
		if (yC < min(currList, (int)WindowSize.ws_row))
		{
			yC++;
			printf("\033[%dB", 1);
		}
		else if (yC < currList)
		{
			printf("\n");
			printf("\033[%dB", 1);
			yC++;
			displayAtIndex(yC - 1);
			cout << "\033[" << yC << ";" << 1 << "H";
		}
		break;
	case 5: //key home
		if (strcmp(currentPath, ROOT_PATH) == 0)
			break;
		prevBackDir.push_back(string(currentPath));
		//strcpy(currentPath, ROOT_PATH);
		prevFwdDir.clear();
		listDirCurrentPath(ROOT_PATH);
		break;
	case 6: //key up one level (parent DIR) (backspace)
		if (strcmp(currentPath, ROOT_PATH) == 0)
			break;
		prevBackDir.push_back(string(currentPath));
		prevFwdDir.clear();
		//strcpy(currentPath, (string(currentPath).substr(0, string(currentPath).find_last_of('/'))).c_str());
		listDirCurrentPath("../");
		break;
	case 7: //key ENTER
		if (yC == currList + 1)
			break;
		dir = listOfDirs[yC - 1];
		fName = string(dir->d_name);
		if (fName == "." || fName == "..")
		{
			if (strcmp(currentPath, ROOT_PATH) == 0)
				break;
			if (fName == ".") //not sure
				break;		  //performOp(2);
			else
				performOp(6);
		}
		else if (typeFileOrDir[yC - 1] == TYPE_DIRECTORY)
		{
			prevBackDir.push_back(string(currentPath));
			//strcpy(currentPath, (string(currentPath) + "/" + fName).c_str());
			listDirCurrentPath((string(currentPath) + "/" + fName).c_str());
		}
		else if (typeFileOrDir[yC - 1] == TYPE_FILE)
		{
			clearScreen();
			string cc;
			if (PLATFORM_NAME == "unix")
				cc = "/usr/bin/vi"; //xdg-open for opening with associated program
			else if (PLATFORM_NAME == "mac")
				cc = "/usr/bin/vi"; //open for opening with associated program
			if (PLATFORM_NAME == "windows")
				cc = "start";
			//execl(cc.c_str(), cc.c_str(), fName.c_str(), NULL);
			pid_t pid;
			if ((pid = fork()) == -1)
				printf("fork error");
			else if (pid == 0)
			{
				execl(cc.c_str(), cc.c_str(), fName.c_str(), NULL); //open each file in def. appl.
				//exit(1);
				printf("execl() error\n");
			}
			else
			{
				wait(NULL);
				listDirCurrentPath(currentPath);
			}
		}
		break;
	case 8: //key command mode
		clearScreen();
		printf("\033[%d;%dH", WindowSize.ws_row, 1);
		printf("copy\tmove\trename\tcreate_file\tcreate_dir\tdelete_file\tdelete_dir\tgoto\tsearch\t\tESC");
		printf("\033[%d;%dH", 1, 1);
		break;
	}
}

vector<string> getTokens(string s)
{
	vector<string> x;
	string t;
	stringstream iss(s);
	while (getline(iss, t, ' '))
		x.push_back(t);
	return x;
}

void navigateNormalMode(int c)
{
	if (c == KEY_ESCAPE) //go to normal mode;
	{
		if (mode == COMMAND_MODE)
		{
			mode = NORMAL_MODE;
			ibc = 0;
			listDirCurrentPath(currentPath);
		}
	}
	else if (c == KEY_RIGHT)
	{
		if (mode == NORMAL_MODE)
			performOp(1);
		//else printf("\033[%dC", 1);
	}
	else if (c == KEY_LEFT)
	{
		if (mode == NORMAL_MODE)
			performOp(2);
		//else printf("\033[%dD", 1);
	}
	else if (c == KEY_UP)
	{
		if (mode == NORMAL_MODE)
			performOp(3);
		//else printf("\033[%dA", 1);
	}
	else if (c == KEY_DOWN)
	{
		if (mode == NORMAL_MODE)
			performOp(4);
		//else printf("\033[%dB", 1);
	}
	else if ((c == 'H' || c == 'h') && mode == NORMAL_MODE) //go to home
	{
		performOp(5);
	}
	else if (c == 127) //backspace = up one level
	{
		if (mode == NORMAL_MODE)
			performOp(6);
		else
		{
			printf("\b \b");
			ibc = max(0, ibc - 1);
		}
	}
	else if (c == KEY_ENTER) //enter - execute file or enter dir
	{
		if (mode == NORMAL_MODE)
			performOp(7);
		else if (ibc > 0)
		{
			string x = "";
			//getline(cin, x);
			for (int i = 0; i < ibc; i++)
				x += inputBuffer[i];
			//cout << "\ntaking " << x << " as input\n";
			ibc = 0;
			printf("\n");
			navigateCommandMode(getTokens(x));
		}
	}
	else if ((c == 'k' || c == 'K') && mode == NORMAL_MODE)//scroll up
	{ 
		for (int i = 1; i <= SCROLL_CONSTANT; i++)
			performOp(3);
	}
	else if ((c == 'l' || c == 'L') && mode == NORMAL_MODE)//scroll down
	{ 
		for (int i = 1; i <= SCROLL_CONSTANT; i++)
			performOp(4);
	}
	else if (c == ':' && mode == NORMAL_MODE) //command mode
	{
		mode = COMMAND_MODE;
		ibc = 0;
		clearScreen();
		performOp(8);
	}
	else if ((c == 'q' || c == 'Q') && mode == NORMAL_MODE) //quit application
	{
		clearScreen();
		exit(1);
	}
	else if (c == EOF) //unrecoverable error
	{
		//cout << "Unrecoverable error\n";
		//exit(0);
	}
	else if (mode == COMMAND_MODE)
	{
		putchar(c);
		inputBuffer[ibc++] = c;
	}
}

const char *pathFormatter(string s) //if ~ is 1st char then assuming absolute path else relative path to currentPath
{
	string pp;
	if (s.at(s.size() - 1) == '/')
		s.erase(s.size() - 1);
	if (s.at(0) == '~')
		pp = ROOT_PATH + s.substr(1);
	else if (s.at(0) == '.')
		pp = string(currentPath);
	else
		pp = string(currentPath) + (s.at(0) == '/' ? "" : "/") + s;
	return pp.c_str();
}

void navigateCommandMode(vector<string> s)
{
	if (s[0] == "copy")
	{
		string dest = s[s.size() - 1];
		if (s.size() < 3)
			printf("\nNot enough arguments\n");
		else
			copyFD(s, pathFormatter(dest), currentPath);
	}
	else if (s[0] == "move")
	{
		string dest = s[s.size() - 1];
		if (s.size() < 3)
			printf("\nNot enough arguments\n");
		else
			moveFD(s, pathFormatter(dest), currentPath);
	}
	else if (s[0] == "rename")
	{
		if (s.size() < 3)
			printf("\nNot enough arguments\n");
		else
			rename(s[1].c_str(), s[2].c_str());
	}
	else if (s[0] == "create_file")
	{
		string dest = s[s.size() - 1];
		if (s.size() < 3)
			printf("\nNot enough arguments\n");
		else
		{
			//printf("%s %s", s[1].c_str(), pathFormatter(dest));
			createFile(s[1].c_str(), pathFormatter(dest));
		}
	}
	else if (s[0] == "create_dir")
	{
		string dest = s[s.size() - 1];
		if (s.size() < 3)
			printf("\nNot enough arguments\n");
		else
			createDir(s[1].c_str(), pathFormatter(dest));
	}
	else if (s[0] == "delete_file")
	{
		if (s.size() < 2)
			printf("\nNot enough arguments\n");
		else
			deleteFD(s[1].c_str());
	}
	else if (s[0] == "delete_dir")
	{
		if (s.size() < 2)
			printf("\nNot enough arguments\n");
		else
			deleteFD(s[1].c_str());
	}
	else if (s[0] == "goto")
	{
		if (s.size() < 2)
			printf("Not enough arguments\n");
		else
		{
			string dest = s[s.size() - 1];
			prevBackDir.push_back(string(currentPath));
			prevFwdDir.clear();
			chdir(pathFormatter(dest));
			getcwd(currentPath, 4096);
		}
	}
	else if (s[0] == "search")
	{
		if (s.size() < 2)
			printf("Not enough arguments\n");
		else
		{
			string dest = s[s.size() - 1];
			bool x = searchFD(dest.c_str(), currentPath);
			if (x)
				printf("%s is present\n", dest.c_str());
			else
				printf("%s is not present\n", dest.c_str());
		}
	}
	else
		cout << "Unknown command\n";
}

void navigate()
{
	int c;
	string line;
	while (true)
	{
		//if (mode == NORMAL_MODE){
		c = kbget();
		navigateNormalMode(c);
		/*}
		else
		{
			getline(cin, line);
			navigateCommandMode(getTokens(line));
		}*/
	}
}

void displayAtIndex(int i)
{
	dirent *de = listOfDirs[i];
	struct stat fileDetails;
	int detailsStatus = stat(de->d_name, &fileDetails);
	mode_t perms = fileDetails.st_mode;
	string permissions = "";
	permissions = permissions + ((S_ISDIR(perms)) ? "d" : "-");
	if (permissions == "d")
		typeFileOrDir.push_back(TYPE_DIRECTORY);
	else
		typeFileOrDir.push_back(TYPE_FILE);
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

	printf("%s \t %-8.8s\t%-8.8s\t%9jd\t%s\t%s", permissions.c_str(), pw->pw_name, gr->gr_name, (intmax_t)fileDetails.st_size, modTime, de->d_name);
}

void listDirCurrentPath(char const *pp)
{
	struct dirent *dee;
	DIR *dr = opendir(pp);
	listOfDirs.clear();
	typeFileOrDir.clear();
	clearScreen();
	chdir(pp);
	getcwd(currentPath, 4096);
	if (dr == NULL)
	{
		printf("Could not open current directory");
		return;
	}
	//printf("Permissions\tOwner\tGroup\tSize(B)\tLast Modified\tName\n");
	while ((dee = readdir(dr)) != NULL)
	{
		listOfDirs.push_back(dee);
	}
	int maxToDisplay = min((int)listOfDirs.size(), (int)WindowSize.ws_row);
	for (int i = 0; i < maxToDisplay; i++)
	{
		displayAtIndex(i);
		if (i != maxToDisplay - 1)
			printf("\n");
		else
			printf("\033[6n");
	}
	currList = listOfDirs.size();
	yC = 1;
	closedir(dr);
	cout << "\033[" << 1 << ";" << 1 << "H"; //move cursor to (1,1)
	fflush(stdout);
}

int main()
{
	//struct winsize size;
	printf("\033[8;3000;1000t"); //maximise window
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &WindowSize);
	//printf("\033[r");//enable scrolling
	fflush(stdout);
	getcwd(ROOT_PATH, 1024);
	CURRENT_ROW = 1;
	//strcpy(currentPath, ROOT_PATH);
	listDirCurrentPath(ROOT_PATH);
	navigate();
	printf("\033[8;%d;%dt", WindowSize.ws_row, WindowSize.ws_col);
	fflush(stdout);
	return 0;
}