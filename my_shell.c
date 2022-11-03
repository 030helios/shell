#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>

#define MAX_LINE 1024 // The maximum length command

int background = 0;

int help(char *args[])
{
    return printf("----------------------------------------------------------\n\
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
----------------------------------------------------------\n");
}
int cd(char *args[])
{
    if (args[1])
        return chdir(args[1]);
    else
        return printf("cd error");
}
int echo(char *args[])
{
    int i = 1;
    if (!strcmp(args[1], "-n"))
        i = 2;
    for (; args[i]; ++i)
    {
        printf("%s", args[i]);
        if (args[i + 1])
            printf(" ");
    }
    if (strcmp(args[1], "-n"))
        printf("\n");
    return 0;
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
int record(char *args[])
{
    printf("history cmd:\n");
    int i = end;
    int j = 1;
    for (; i < end + 16; ++i)
        if (!isBlank(last16[i % 16]))
            printf("%d: %s\n", j++, last16[i % 16]);
    return 0;
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
int mypid(char *args[])
{
    char *flag = args[1];
    if (!strcmp(flag, "-i"))
        printf("%d\n", getpid());
    else
    {
        int id = atoi(args[2]);
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
    return 0;
}

int execute(char *args[])
{
    int pid = fork();
    if (pid < 0)
        fprintf(stderr, "Fork Failed");
    else if (pid == 0) /* child process */
        execvp(args[0], args);
    else /* parent process */
        if (!background)
            waitpid(pid, NULL, 0);
    return pid;
}

/**
 * Redirects stdin from a file.
 *
 * @param fileName the file to redirect from
 */
void redirectIn(char *fileName)
{
    int in = open(fileName, O_RDONLY);
    dup2(in, 0);
    close(in);
}

/**
 * Redirects stdout to a file.
 *
 * @param fileName the file to redirect to
 */
void redirectOut(char *fileName)
{
    int out = open(fileName, O_WRONLY | O_TRUNC | O_CREAT, 0600);
    dup2(out, 1);
    close(out);
}

int isBuiltin(char *line)
{
    return !strcmp(line, "help") || !strcmp(line, "cd") || !strcmp(line, "echo") || !strcmp(line, "record") || !strcmp(line, "mypid") || !strcmp(line, "replay");
}

int runBuiltin(char *args[])
{
    if (!strcmp(args[0], "help"))
        return help(args);
    else if (!strcmp(args[0], "cd"))
        return cd(args);
    else if (!strcmp(args[0], "echo"))
        return echo(args);
    else if (!strcmp(args[0], "record"))
        return record(args);
    else if (!strcmp(args[0], "mypid"))
        return mypid(args);
}

/**
 * Runs a command.
 *
 * @param *args[] the args to run
 */
int run(char *args[])
{
    int ret;
    if (isBuiltin(args[0]))
        if (background)
        {
            ret = fork();
            if (ret == 0)
            {
                runBuiltin(args);
                exit(0);
            }
        }
        else
            runBuiltin(args);
    else
        ret = execute(args);
    redirectIn("/dev/tty");
    redirectOut("/dev/tty");
    return ret;
}

/**
 * Creates a pipe.
 *
 * @param args [description]
 */
void createPipe(char *args[])
{
    int fd[2];
    pipe(fd);

    dup2(fd[1], 1);
    close(fd[1]);

    run(args);

    dup2(fd[0], 0);
    close(fd[0]);
}

void runline(char *originalLine)
{
    char line[MAX_LINE];
    strcpy(line, originalLine);
    // record
    strcpy(last16[end % 16], line);
    end = (end + 1) % 16;
    // handle background
    if (line[strlen(line) - 1] == '&')
    {
        background = 1;
        line[strlen(line) - 1] = '\0';
    }
    // command line arguments
    char *args[MAX_LINE];
    char *arg = strtok(line, " ");
    int i = 0;
    while (arg)
    {
        if (*arg == '<')
            redirectIn(strtok(NULL, " "));
        else if (*arg == '>')
            redirectOut(strtok(NULL, " "));
        else if (*arg == '|')
        {
            args[i] = NULL;
            createPipe(args);
            i = 0;
        }
        else
        {
            args[i] = arg;
            i++;
        }
        arg = strtok(NULL, " ");
    }
    args[i] = NULL;
    if (background)
        printf("%d\n", run(args));
    else
        run(args);
}

void replay(char *line)
{
    int id;
    sscanf(&line[7], "%d", &id);
    --id;
    if (id > -1 && id < 16)
        if (!isBlank(last16[id]))
            return runline(last16[id]);
    printf("wrong args\n");
}

/**
 * Runs a basic shell.
 *
 * @return 0 upon completion
 */
int main(void)
{
    int i;
    for (i = 0; i < 16; ++i)
    {
        last16[i] = malloc(MAX_LINE);
        last16[i][0] = '\0';
    }
    printf("========\nMY SHELL\n========\n");
    while (1)
    {
        printf(">>> $");
        fflush(stdout);

        char line[MAX_LINE];
        fgets(line, MAX_LINE, stdin);
        while (isspace(line[strlen(line) - 1]))
            line[strlen(line) - 1] = '\0';

        if (isBlank(line) == 1)
            continue;

        background = line[strlen(line) - 1] == '&';

        char firstCmd[MAX_LINE];
        sscanf(line, "%s", firstCmd);

        if (!strcmp(firstCmd, "replay"))
            if (background)
            {
                int pid = fork();
                if (pid == 0)
                {
                    replay(line);
                    exit(0);
                }
                printf("%d\n", pid);
            }
            else
                replay(line);
        else if (!strcmp(firstCmd, "exit"))
        {
            printf("byebye\n");
            return 0;
        }
        else
            runline(line);
        background = 0;
    }
    return 0;
}