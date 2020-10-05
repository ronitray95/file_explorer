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
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <stack>
#include <vector>

using namespace std;

void copyFD(vector<string>, char const *, char *);

void copyFile(char const *absPath, dirent *fCopy)
{
	unsigned char block[4096];
	int in, out;
	int nread;
	in = open(fCopy->d_name, O_RDONLY);
	out = open(absPath, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	while (true)
	{
		int err = read(in, block, 4096);
		if (err == -1)
		{
			printf("Error reading file.\n");
			break;
		}
		if (err == 0)
			break;
		err = write(out, block, err);
		if (err == -1)
		{
			printf("Error writing to file.\n");
			exit(1);
		}
	}
	close(in);
	close(out);
}

void copyFD(vector<string> files, char const *absPath, char *curr)
{
	for (int i = 1; i < files.size() - 1; i++)
	{
		string fName = files[i];
		struct stat fileDetails, locDetails;
		dirent *fCopy = readdir(opendir((string(curr) + fName).c_str()));
		dirent *toCopy = readdir(opendir(absPath));
		int fD = stat(fCopy->d_name, &fileDetails), lD = stat(fCopy->d_name, &locDetails);
		mode_t fP = fileDetails.st_mode, lP = locDetails.st_mode;
		if (!(fP & S_IRUSR))
		{
			printf("Cannot read file %s\n", fName.c_str());
			continue;
		}
		if (!(lP & S_IWUSR))
		{
			printf("Do not have write permission for location\n");
			break;
		}
		if (S_ISDIR(fP))
		{
			DIR *dp = opendir(fName.c_str());
			struct dirent *entry;
			struct stat statbuf;

			if (dp == NULL)
			{
				printf("Error opening the directory %s", fName.c_str());
				return;
			}
			chdir(fName.c_str());
			while ((entry = readdir(dp)) != NULL)
			{
				stat(entry->d_name, &statbuf);
				if (S_ISDIR(statbuf.st_mode))
				{
					if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
						continue;
					mkdir((string(absPath) + '/' + string(entry->d_name)).c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
					vector<string> newd;
					newd.push_back(string(entry->d_name));
					copyFD(newd, (string(absPath) + '/' + string(entry->d_name)).c_str(), curr);
				}
				else
				{
					copyFile(entry->d_name, entry);
				}
			}
			chdir("..");
			closedir(dp);
		}
		else
			copyFile(absPath, fCopy);
	}
}

void createFile(const char *f, const char *loc)
{
	int x = open((string(loc) + "/" + string(f)).c_str(), O_RDWR | O_CREAT, S_IXUSR | S_IRUSR | S_IXUSR);
	if (x == -1)
		printf("Error creating file\n");
}

void createDir(const char *f, const char *loc)
{
	int x = mkdir((string(loc) + "/" + string(f)).c_str(), S_IXUSR | S_IRUSR | S_IXUSR);
	if (x == -1)
		printf("Error creating directory\n");
}