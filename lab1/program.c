
#include <stdio.h>
#include <dirent.h>

int main(int argc, char *argv[])
{
    DIR *dir;
    struct dirent *de;
    if ((dir = opendir(argv[1])) != NULL)
    {
        while ((de = readdir(dir)) != NULL)
            printf("%s\n", de->d_name);
    }
    else
    {
        printf("%s\n", "nu se poate deschide");
    }

    closedir(dir);
}