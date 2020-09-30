#include <termios.h>
#include <iostream>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include <list>
#include <unistd.h>
#include <sys/errno.h>
#include <string>
#include <locale.h>
#include <langinfo.h>
#include <time.h>

using namespace std;
int main()
{
	struct dirent *de;
	struct stat fileDetails;
	list<dirent *> listOfDirs;
	DIR *dr = opendir(".");
	if (dr == NULL)
	{
		printf("Could not open current directory");
		return 0;
	}
	printf("Permissions\tOwner\tGroup\tSize(B)\tLast Modified\tName\n");
	while ((de = readdir(dr)) != NULL)
	{
		listOfDirs.push_back(de);
		int detailsStatus = stat(de->d_name, &fileDetails);
		mode_t perms = fileDetails.st_mode;
		struct passwd *pw = getpwuid(fileDetails.st_uid);
		struct group *gr = getgrgid(fileDetails.st_gid);
		struct tm *tm = localtime(&fileDetails.st_mtime);
		string owner = (pw == NULL ? "Undefined" : pw->pw_name);
		string grp = (gr == NULL ? "Undefined" : gr->gr_name);
		char modTime[256];

		strftime(modTime, sizeof(modTime), nl_langinfo(D_T_FMT), tm);

		printf(" (%3o)\t%-8.8s\t%-8.8s\t%9jd\t%s\t%s\n", perms & 0777, pw->pw_name, gr->gr_name, (intmax_t)fileDetails.st_size, modTime, de->d_name);
	}
	closedir(dr);
	return 0;
}