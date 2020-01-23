/* attempt to get multiple variables with the same name alive at the same time */
#include <stdlib.h>
#include <stdio.h>

static
int fact(int i) {
	int result = 1;
	while (i) {
		result = result * i;
		i--;
	}
	return result;
}

int main(int argc, char *argv[])
{
	int i, j;
	sscanf(argv[1], "%d", &i);
	printf("i = %d\n", i);
	j = fact(i);
	printf("%d = factorial(%d)\n", j, i);
	exit(0);
}
