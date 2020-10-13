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
	struct stat fileProps;
	stat(fCopy->d_name, &fileProps);
	in = open(fCopy->d_name, O_RDONLY);
	string dest=string(absPath)+"/"+string(fCopy->d_name);
	out = open(dest.c_str(), O_WRONLY | O_CREAT, fileProps.st_mode);
	//printf("Copy file %s to %s", fCopy->d_name, absPath);
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
			break;
		}
	}
	close(in);
	close(out);
}

void copyDir(char const *absPath, dirent *dir)
{
	struct stat dirDetails, entryDetails;
	struct dirent *entry;
	string copyPath = (string(absPath) + "/" + string(dir->d_name));
	int x = mkdir(copyPath.c_str(), 0);
	stat(dir->d_name, &dirDetails);
	if (x == -1)
	{
		//printf("%s %s\n", absPath, dir->d_name);
		printf("Error creating directory %s \n", copyPath.c_str());
		return;
	}
	else
		chmod(copyPath.c_str(), dirDetails.st_mode);
	DIR *dp = opendir(dir->d_name);
	if (dp == NULL)
	{
		printf("Error opening the directory %s\n", dir->d_name);
		return;
	}
	chdir(dir->d_name);
	while ((entry = readdir(dp)) != NULL)
	{
		stat(entry->d_name, &entryDetails);
		if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
			continue;
		else if (S_ISDIR(entryDetails.st_mode))
			copyDir(copyPath.c_str(), entry);
		else
			copyFile(copyPath.c_str(), entry);
	}
	chdir("..");
	closedir(dp);
}

void copyFD(vector<string> files, char const *absPath, char *curr)
{
	for (int i = 1; i < files.size() - 1; i++)
	{
		string fName = files[i];
		DIR *dp = opendir(curr);
		struct dirent *entry;
		struct stat statbuf;
		if (dp == NULL)
		{
			printf("Error opening the current directory\n");
			return;
		}
		//cout << "For " << fName << " entries:\n";
		while ((entry = readdir(dp)) != NULL)
		{
			stat(entry->d_name, &statbuf);
			mode_t fP = statbuf.st_mode;
			if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
				continue;
			else if (strcmp(entry->d_name, fName.c_str()) == 0 && S_ISDIR(fP))
			{
				//cout << "Attempting copy dir " << entry->d_name << "\n";
				copyDir(absPath, entry);
			}
			else if (strcmp(entry->d_name, fName.c_str()) == 0)
			{
				//cout << "Attempting copy file " << entry->d_name << "\n";
				copyFile(absPath, entry);
			}
			//return searchFD(fName, entry->d_name);
		}
		closedir(dp);
	}
}

void createFile(const char *f, const char *loc)
{
	int x = open((string(loc) + "/" + string(f)).c_str(), O_RDWR | O_CREAT, 0); // S_IRUSR | S_IXUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
	if (x == -1)
		printf("Error creating file\n");
	else
		chmod((string(loc) + "/" + string(f)).c_str(), 0777);
}

void createDir(const char *f, const char *loc)
{
	int x = mkdir((string(loc) + "/" + string(f)).c_str(), 0); //S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
	if (x == -1)
		printf("Error creating directory\n");
	else
		chmod((string(loc) + "/" + string(f)).c_str(), 0777);
}

void deleteFD(const char *fName)
{
	int z = remove(fName);
	if (z != 0)
	{
		DIR *dp = opendir(fName);
		struct dirent *entry;
		struct stat statbuf;
		if (dp == NULL)
		{
			printf("Error opening the directory %s", fName);
			return;
		}
		chdir(fName);
		while ((entry = readdir(dp)) != NULL)
		{
			if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
				continue;
			else
				deleteFD(entry->d_name);
		}
		chdir("..");
		closedir(dp);
		if (remove(fName) != 0)
			printf("Deletion of %s failed\n", fName);
	}
}

void moveFD(vector<string> files, char const *absPath, char *curr)
{
	copyFD(files, absPath, curr);
	for (int i = 1; i < files.size() - 1; i++)
		deleteFD(files[i].c_str());
}

bool searchFD(const char *fName, char const *curr)
{
	DIR *dp = opendir(curr);
	struct dirent *entry;
	struct stat statbuf;
	if (dp == NULL)
	{
		//printf("Error opening the current directory\n");
		return false;
	}
	chdir(fName);
	vector<dirent *> dirList;
	while ((entry = readdir(dp)) != NULL)
	{
		stat(entry->d_name, &statbuf);
		mode_t fP = statbuf.st_mode;
		if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
			continue;
		else if (strcmp(entry->d_name, fName) == 0)
		{
			return true;
		}
		else if (S_ISDIR(fP))
			dirList.push_back(entry);
		//return searchFD(fName, entry->d_name);
	}
	closedir(dp);
	for (int i = 0; i < dirList.size(); i++)
	{
		entry = dirList[i];
		bool b = searchFD(fName, entry->d_name);
		if (b)
			return true;
	}
	chdir("..");
	return false;
}