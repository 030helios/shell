#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>

void help(char *line)
{
	printf("----------------------------------------------------------\n\
			my little shell\n\
			Type program names and arguments and hit enter\n\n\
			The following are built in:\n\
			1: help		show all built-in function info\n\
			2: cd		change directory\n\
			3: echo		echo the strings to standard output\n\
			4: record	show last-16 cmds you typed in\n\
			5: replay	re-execute the cmd showed in record\n\
			6: mypid	find and print process-ids\n\
			7: exit		exit shell\n\n\
			Use the \"man\" command for information on other programs.\n\
			----------------------------------------------------------\n\
			");
}
void cd(char *line)
{
	char dir[100];
	sscanf(&line[3], "%s", dir);
	chdir(dir);
}
void echo(char *line)
{
	char flag[100];
	sscanf(&line[5], "%s", flag);
	if (!strcmp(flag, "-n"))
		printf("%s", &line[8]);
	else
		printf("%s\n", &line[5]);
}
int isBlank(char *line)
{
	char *ch;
	for (ch = line; *ch != '\0'; ++ch)
		if (!isspace(*ch))
			return 0;
	return 1;
}
int end = 0;
char *last16[16];
void record(char *line)
{
	printf("history cmd:\n");
	int i = end;
	int j = 1;
	for (; i < end + 16; ++i)
		if (!isBlank(last16[i % 16]))
			printf("%d: %s\n", j++, last16[i % 16]);
}

int ppid(int pid)
{
	int ppid;
	char buf[100];
	char procname[32]; // Holds /proc/4294967296/status\0
	FILE *fp;
	snprintf(procname, sizeof(procname), "/proc/%u/status", pid);
	fp = fopen(procname, "r");
	if (fp != NULL)
	{
		size_t ret = fread(buf, sizeof(char), 100 - 1, fp);
		if (!ret)
		{
			return 0;
		}
		else
		{
			buf[ret++] = '\0'; // Terminate it.
		}
	}
	fclose(fp);
	char *ppid_loc = strstr(buf, "\nPPid:");
	if (ppid_loc)
	{
		ppid = sscanf(ppid_loc, "\nPPid:%d", &ppid);
		if (!ppid || ppid == EOF)
		{
			return 0;
		}
		return ppid;
	}
	else
	{
		return 0;
	}
}
int isnumber(char *p)
{
	for (; *p; p++)
		if (!isdigit(*p))
			return 0;
	return 1;
}
void mypid(char *line)
{
	char flag[100];
	sscanf(&line[5], "%s", flag);
	if (!strcmp(flag, "-i"))
		printf("%d\n", getpid());
	else
	{
		int id;
		sscanf(&line[9], "%d", &id);
		if (!strcmp(flag, "-p"))
			printf("%d\n", ppid(id));
		else if (!strcmp(flag, "-c"))
		{
			struct dirent *entry;
			DIR *procdir = opendir("/proc");
			// Iterate through all files and directories of /proc.
			while ((entry = readdir(procdir)))
				if (isnumber(entry->d_name))
				{
					int pid;
					sscanf(entry->d_name, "%d", &pid);
					if (id == ppid(pid))
						printf("%d\n", pid);
				}
			closedir(procdir);
		}
	}
}

void execute(char *line)
{
	// a copy of line for strtok
	char tokenizedLine[100];
	strcpy(tokenizedLine, line);
	// the arguments of line including operators
	// no new strings are initialized
	char *args[100] = {NULL};
	args[0] = strtok(tokenizedLine, " ");
	int n = 0;
	while (args[n] != NULL)
		args[++n] = strtok(NULL, " ");

	if (!strcmp(args[0], "help"))
		help(line);
	else if (!strcmp(args[0], "cd"))
		cd(line);
	else if (!strcmp(args[0], "echo"))
		echo(line);
	else if (!strcmp(args[0], "record"))
		record(line);
	else if (!strcmp(args[0], "mypid"))
		mypid(line);
	else
	{
		int pid;
		if (pid = fork())
			waitpid(pid, NULL, 0);
		else
			execvp(args[0], args);
	}
}

void piper(char *line)
{
	// a copy of line for strtok
	char tokenizedLine[100];
	strcpy(tokenizedLine, line);
	// tokenize with pipe
	char *args[100] = {strtok(tokenizedLine, "|")};
	// amount of commands
	int n = 1;
	for (; args[n] != NULL; ++n)
	{
		// since we are tokenizing the string with |
		// the command after | will begin with a space
		if (args[n] = strtok(NULL, "|"))
			++args[n];
	}
	int cin = dup(0), cout = dup(1);
	// the first and last pipe should be stdio
	int fd[2 * (n - 1)];
	int i = 0;
	for (; i < n; ++i)
		if (i != (n - 1))
		{
			pipe(&fd[2 * i]);
			dup2(fd[2 * i], STDOUT_FILENO);
			execute(args[i]);
			dup2(fd[2 * i + 1], STDIN_FILENO);
			close(fd[2 * i]);
			close(fd[2 * i - 1]);
		}
		else
		{
			dup2(cout, 1);
			execute(args[i]);
			dup2(cin, 0);
		}
}

void replay(char *line)
{
	int id;
	sscanf(&line[7], "%d", &id);
	--id;
	if (id > -1 && id < 16 && id < end)
		if (!isBlank(last16[id]))
			return execute(last16[id]);
	printf("wrong args\n");
}
int main()
{
	int i;
	for (i = 0; i < 16; ++i)
	{
		last16[i] = malloc(100);
		last16[i][0] = '\0';
	}

	printf("========\nMY SHELL\n========\n");
	char line[100];
	while (1)
	{
		printf(">>> $");
		fgets(line, 100, stdin);
		line[strlen(line) - 1] = '\0';
		if (isBlank(line) == 1)
			continue;

		char cmd[100];
		sscanf(line, "%s", cmd);

		if (!strcmp(cmd, "replay"))
			replay(line);
		else if (!strcmp(cmd, "exit"))
			return printf("bye\n");
		else
		{
			// record
			strcpy(last16[end % 16], line);
			end = (end + 1) % 16;
			piper(line);
		}
	}
	return 0;
}
