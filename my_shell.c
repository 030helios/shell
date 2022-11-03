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

// Terminate if this process is forked
int isOriginalProcess = 1;

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

int isCmd(char *line, char *type)
{
	char cmd[100];
	sscanf(line, "%s", cmd);
	return !strcmp(cmd, type);
}

int isBuiltin(char *line)
{
	return isCmd(line, "help") || isCmd(line, "cd") || isCmd(line, "echo") || isCmd(line, "record") || isCmd(line, "mypid");
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

void redirector(char *line)
{
	int cin = dup(STDIN_FILENO), cout = dup(STDOUT_FILENO);
	char *outfile, *infile;
	// scan for redirect out and modify line
	if (outfile = strstr(line, " > "))
	{
		*outfile = '\0';
		FILE *out = fopen(outfile + 3, "w");
		dup2(fileno(out), STDOUT_FILENO);
	}
	// scan for redirect in and modify line
	if (infile = strstr(line, " < "))
	{
		*infile = '\0';
		FILE *in = fopen(infile + 3, "r");
		dup2(fileno(in), STDIN_FILENO);
	}
	execute(line);
	dup2(cin, 0);
	dup2(cout, 1);
}

void piper(char *line)
{
	char *args[100] = {line};
	int n = 0;
	// note that we are modifying line. This is basically handmade strtok.
	while (args[++n] = strstr(args[n], " | "))
	{
		*args[n] = '\0';
		args[n] += 3;
	}

	// restore STDIO to its default after this
	int cin = dup(0), cout = dup(1);
	int i = 0, fd[2 * (n - 1)];
	for (; i < n; ++i)
		if (i != (n - 1)) // the ith command pipes fd[2*i] and fd[2*i+1]
		{
			pipe(&fd[2 * i]);
			dup2(fd[2 * i + 1], STDOUT_FILENO);
			// try redirect if we are processing args[0]
			if (i)
				execute(args[i]);
			else
				redirector(args[0]);
			// close the output so that the next command doesn't hold
			close(fd[2 * i + 1]);
			dup2(fd[2 * i], STDIN_FILENO);
		}
		else // the last command restores STDIO to its default
		{
			dup2(cout, STDOUT_FILENO);
			redirector(args[i]);
			dup2(cin, STDIN_FILENO);
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
	while (isOriginalProcess)
	{
		printf(">>> $");
		fgets(line, 100, stdin);
		while (isspace(line[strlen(line) - 1]))
			line[strlen(line) - 1] = '\0';
		if (isBlank(line) == 1)
			continue;

		if (isCmd(line, "replay"))
			replay(line);
		else if (isCmd(line, "exit"))
			return printf("bye\n");
		else
			piper(line);
	}
	return 0;
}
