#include <stdio.h>
#include <string.h>

int extractPercentage(char buffer[256]) {
    const char *percentStr = strstr(buffer, "%"); // Find the '%' character
    if (percentStr != NULL) {
        int percent;
        sscanf(percentStr - 3, "%d", &percent); // Assuming the percentage is always two digits
        return percent;
    }
    return -1; // If '%' is not found
}

int main() {
    char inputString[256] = "/usr/bin/shred: /dev/sdb : étape 1/1 (random)…61MiB/1,0GiB 5 %";

    int percent = extractPercentage(inputString);

    if (percent != -1) {
        printf("Percentage: %d%%\n", percent);
    } else {
        printf("Percentage not found\n");
    }

    return 0;
}

