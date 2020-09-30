#include <termios.h>
#include <iostream>
#include <dirent.h>
#include <list>
#include <sys/errno.h>
#include <string>

using namespace std;
int main()
{
	struct dirent *de;
	list<dirent *> listOfDirs;
	DIR *dr = opendir(".");
	if (dr == NULL)
	{
		printf("Could not open current directory");
		return 0;
	}
	while ((de = readdir(dr)) != NULL){
		listOfDirs.push_back(de);
		printf("%s\n", de->d_name);
	}
	closedir(dr);
	return 0;
}