#include <stdio.h>

struct Directory {
    int id;
    struct DirectoryNode* first;
    struct DirectoryNode* second;
    struct DirectoryNode* third;
};

struct DirectoryNode {
    char * someInfo;
    struct Directory* directory;
};

void directoryFunc(struct Directory* dir);
void nodeFunc(struct DirectoryNode* node);

void directoryFunc(struct Directory* dir) {
    if (dir == NULL) {
        return;
    }
    printf("%d\n", dir->id);

    nodeFunc(dir->first);
    nodeFunc(dir->second);
    nodeFunc(dir->third);
}

void nodeFunc(struct DirectoryNode* node) {
    if (node == NULL) {
        return;
    }

    printf("%s\n", node->someInfo);

    directoryFunc(node->directory);
}

int main(void) {
    struct Directory dFirst;
    dFirst.id = 1;

    // "Lower Level" ones
    struct Directory lowerOne;
    lowerOne.id = 2;
    lowerOne.first = NULL;
    lowerOne.second = NULL;
    
    struct Directory lowerTwo;
    lowerTwo.id = 3;
    lowerTwo.first = NULL;
    lowerTwo.second = NULL;

    // Directory Nodes
    struct DirectoryNode first;
    first.someInfo = "dir1\0";
    first.directory = &lowerOne;

    struct DirectoryNode second;
    second.someInfo = "dir2\0";
    second.directory = &lowerOne;

    struct DirectoryNode third;
    third.someInfo = "dir3\0";
    third.directory = NULL;

    dFirst.first = &first;
    dFirst.second = &second;
    dFirst.third = &third;

    // Make it a bit goofy lol, include some repeats
    lowerOne.third = &third;
    lowerTwo.third = &third;

    directoryFunc(&dFirst);
    return 1;
}