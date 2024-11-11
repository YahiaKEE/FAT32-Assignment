//Yahia Elsaad
//11/11/2024
//3320 Operating System


#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_FILENAME 100
#define DIRECTORY_ENTRY_SIZE 32

// FAT32 BPB fields
unsigned short BPB_BytesPerSec;
unsigned char BPB_SecPerClus;
unsigned short BPB_RsvdSecCnt;
unsigned char BPB_NumFATs;
unsigned int BPB_FATSz32;
unsigned int BPB_RootClus;

// File pointer for the FAT32 image file
FILE *fp = NULL;
unsigned int currentCluster;  // Track the current directory cluster

// Define the DirectoryEntry structure
typedef struct {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t unused[20];
    uint16_t DIR_FstClusHI;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} DirectoryEntry;
char Open_Filename[MAX_FILENAME] = "";
// Array for storing directory entries (fixed size for this example)
DirectoryEntry dir[16];

// Function declarations
void handle_open(const char *filename);
void handle_close();
void handle_info();
void handle_ls();
void handle_cd(const char *dirname);
void handle_stat(const char *name);
void handle_put(const char *localFilename, const char *newFilename);
void handle_get(const char *filename, const char *newFilename);
void handle_read(const char *filename, int position, int num_bytes, const char *option);
void handle_del(const char *filename);
void Set_FATEntry(unsigned int cluster, unsigned int value);
unsigned int Get_NextCluster(unsigned int cluster);
void parse_bpb_info();
void prompt();
unsigned int find_directory_entry(const char *name, unsigned int cluster, char *entry);
unsigned int find_empty_directory_entry(unsigned int cluster, char *entry);
unsigned int find_empty_cluster();
void write_to_cluster(unsigned int cluster, const char *data, size_t size);

// Prototype for readDirectory
void readDirectory(unsigned int cluster);

int main() {
    char command[MAX_FILENAME + 10]; // Buffer to store user command input
    char *token;                     // Pointer for tokenizing the command input

    prompt(); // Display initial prompt to user
    while (fgets(command, sizeof(command), stdin)) { // Read user input line by line
        command[strcspn(command, "\n")] = 0;  // Remove the trailing newline character
        token = strtok(command, " ");         // Tokenize the input to extract command keyword

        if (token == NULL) {   // If input is empty, prompt again
            prompt();
            continue;
        }

        // Command handling based on keyword
        if (strcasecmp(token, "open") == 0) {  // Check if command is "open"
            token = strtok(NULL, " ");         // Get the filename
            if (token == NULL) {
                printf("No filename specified.\n"); // Error if no filename
            } else {
                handle_open(token);            // Call handle_open with filename
            }
        } else if (strcasecmp(token, "close") == 0) { // Check if command is "close"
            handle_close();                    // Call handle_close to close filesystem
        } else if (strcasecmp(token, "info") == 0) {  // Check if command is "info"
            handle_info();                     // Call handle_info to display filesystem info
        } else if (strcasecmp(token, "ls") == 0) {    // Check if command is "ls"
            handle_ls();                       // Call handle_ls to list directory contents
        } else if (strcasecmp(token, "cd") == 0) {    // Check if command is "cd"
            token = strtok(NULL, " ");         // Get the directory name
            if (token == NULL) {
                printf("There was no directory name specified.\n"); // Error if no directory name
            } else {
                handle_cd(token);              // Call handle_cd with directory name
            }
        } else if (strcasecmp(token, "stat") == 0) {  // Check if command is "stat"
            token = strtok(NULL, " ");         // Get the file or directory name
            if (token == NULL) {
                printf("The file or directory name was not specified.\n"); // Error if name not specified
            } else {
                handle_stat(token);            // Call handle_stat with name
            }
        } else if (strcasecmp(token, "put") == 0) {   // Check if command is "put"
            char *localFilename = strtok(NULL, " ");  // Get the local filename
            char *newFilename = strtok(NULL, " ");    // Optional new filename in the filesystem
            if (localFilename == NULL) {
                printf("There was no filename specified.\n"); // Error if no local filename
            } else {
                handle_put(localFilename, newFilename ? newFilename : localFilename); // Call handle_put
            }
        } else if (strcasecmp(token, "read") == 0) {  // Check if command is "read"
            char *filename = strtok(NULL, " ");       // Get filename to read from
            char *pos_str = strtok(NULL, " ");        // Get start position for reading
            char *num_bytes_str = strtok(NULL, " ");  // Get number of bytes to read
            char *option = strtok(NULL, " ");         // Get optional format option for output

            if (filename == NULL || pos_str == NULL || num_bytes_str == NULL) {
                printf("You are missing arguments for read command.\n"); // Error if missing arguments
            } else {
                int position = atoi(pos_str);         // Convert position to integer
                int num_bytes = atoi(num_bytes_str);  // Convert number of bytes to integer
                handle_read(filename, position, num_bytes, option); // Call handle_read
            }
        } else if (strcasecmp(token, "del") == 0) {   // Check if command is "del"
            token = strtok(NULL, " ");                // Get filename to delete
            if (token == NULL) {
                printf("There was no filename specified.\n"); // Error if no filename
            } else {
                handle_del(token);                    // Call handle_del to delete file
            }
        } else if (strcasecmp(token, "quit") == 0 || strcasecmp(token, "exit") == 0) { // Check if "quit" or "exit"
            handle_close();                           // Ensure filesystem is closed before exiting
            break;                                    // Exit the command loop
        } else {
            printf("Invalid command.\n");             // Print error for unrecognized command
        }

        prompt(); // Display prompt again for next command
    }

    return 0; // End of main function
}

void handle_get(const char *filename, const char *newFilename) {
    if (fp == NULL) { //Error checking
        printf("Error: File system not open.\n");
        return;
    }

    // Search for the file entry in the current directory
    char entry[DIRECTORY_ENTRY_SIZE];
    unsigned int cluster = find_directory_entry(filename, currentCluster, entry);

    if (cluster == 0) {  // If no file entry found
        printf("Error: File not found.\n");
        return;
    }

    // Determine the file size from the directory entry
    unsigned int fileSize = *(unsigned int *)&entry[28];
    if (fileSize == 0) {  // Check if the file is empty
        printf("Error: Cannot retrieve an empty file.\n");
        return;
    }

    // Decide the output filename: use the original or new name if provided
    const char *outputFilename = (newFilename == NULL) ? filename : newFilename;

    // Open the file in the current directory for writing
    FILE *outputFile = fopen(outputFilename, "wb");
    if (outputFile == NULL) {
        printf("Error: Could not create output file.\n");
        return;
    }

    // Buffer to read the data
    char *buffer = (char *)malloc(BPB_BytesPerSec * BPB_SecPerClus);
    if (buffer == NULL) {
        printf("Error: Memory allocation failed.\n");
        fclose(outputFile);
        return;
    }

    // Read and write data from the FAT32 image file cluster-by-cluster
    unsigned int remainingBytes = fileSize;
    unsigned int currentCluster = cluster;

    while (remainingBytes > 0 && currentCluster < 0x0FFFFFF8) {
        // Calculate bytes to read for the current cluster (could be less than cluster size on the last read)
        unsigned int bytesToRead = (remainingBytes < BPB_BytesPerSec * BPB_SecPerClus) ? remainingBytes : BPB_BytesPerSec * BPB_SecPerClus;

        // Move to the cluster's sector in the FAT32 image
        unsigned int sectorOffset = ((BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32)) * BPB_BytesPerSec) + (currentCluster - 2) * BPB_SecPerClus * BPB_BytesPerSec;
        fseek(fp, sectorOffset, SEEK_SET);
        fread(buffer, 1, bytesToRead, fp);

        // Write to the output file
        fwrite(buffer, 1, bytesToRead, outputFile);

        // Move to the next cluster
        remainingBytes -= bytesToRead;
        currentCluster = Get_NextCluster(currentCluster);
    }

    // Clean up
    free(buffer);
    fclose(outputFile);

    printf("File '%s' successfully retrieved as '%s'.\n", filename, outputFilename);
}



void handle_del(const char *filename) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        return;
    }

    char formattedName[12];
    memset(formattedName, ' ', 11);
    strncpy(formattedName, filename, strlen(filename));
    formattedName[11] = '\0';  // Null-terminate for safety

    bool found = false;

    // Read the current directory cluster
    readDirectory(currentCluster);

    for (int i = 0; i < 16; i++) {
        if (strncmp(dir[i].DIR_Name, formattedName, 11) == 0) {
            // Mark file as deleted by setting the first byte to 0xE5
            dir[i].DIR_Name[0] = 0xE5;

            // Calculate offset for the directory entry in the FAT32 image
            unsigned int entryOffset = ((BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32)) * BPB_BytesPerSec) +
                                       (currentCluster - 2) * BPB_SecPerClus * BPB_BytesPerSec + (i * DIRECTORY_ENTRY_SIZE);

            // Seek to the entry's location in the FAT32 image file
            fseek(fp, entryOffset, SEEK_SET);
            fwrite(&dir[i], DIRECTORY_ENTRY_SIZE, 1, fp);
            found = true;

            // Clear clusters in the FAT
            unsigned int cluster = (dir[i].DIR_FstClusHI << 16) | dir[i].DIR_FstClusLO;
            while (cluster < 0x0FFFFFF8) {
                unsigned int nextCluster = Get_NextCluster(cluster);
                Set_FATEntry(cluster, 0); // Set the FAT entry to 0 to mark it as free
                cluster = nextCluster;
            }

            printf("File '%s' deleted successfully.\n", filename);
            break;
        }
    }

    if (!found) {
        printf("Error: File not found.\n");
    }
}


void Set_FATEntry(unsigned int cluster, unsigned int value) {
    unsigned int fatOffset = BPB_RsvdSecCnt * BPB_BytesPerSec + cluster * 4;
    fseek(fp, fatOffset, SEEK_SET);
    fwrite(&value, sizeof(value), 1, fp);
}

unsigned int Get_NextCluster(unsigned int cluster) {
    // Calculate the FAT offset based on the cluster number
    unsigned int fatOffset = BPB_RsvdSecCnt * BPB_BytesPerSec + (cluster * 4);

    // Seek to the correct position in the FAT for this cluster's entry
    fseek(fp, fatOffset, SEEK_SET);

    // Read the next cluster value
    unsigned int nextCluster;
    fread(&nextCluster, sizeof(nextCluster), 1, fp);

    // Mask with 0x0FFFFFFF to get the 28-bit cluster number (FAT32 uses 28 bits for clusters)
    return nextCluster & 0x0FFFFFFF;
}


void readDirectory(unsigned int cluster) {
    // Calculate the sector offset for the current directory cluster
    unsigned int firstDataSector = BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32);
    unsigned int clusterOffset = cluster - 2;  // Subtract 2 because FAT32 cluster numbering starts at 2
    unsigned int sectorOffset = (firstDataSector + (clusterOffset * BPB_SecPerClus)) * BPB_BytesPerSec;
    
    fseek(fp, sectorOffset, SEEK_SET);
    
    // Read 16 directory entries (assuming each entry is 32 bytes and we want 16 entries)
    for (int i = 0; i < 16; i++) {
        fread(&dir[i], DIRECTORY_ENTRY_SIZE, 1, fp);
    }
}
// Display prompt
void prompt() {
    printf("mfs> ");
}

// Open FAT32 file system image
// Function to open a FAT32 file system image
void handle_open(const char *filename) {
    if (fp != NULL) {  // Check if there's already an open file
        printf("A file system image is already open. Close it first.\n");
        return;
    }

    // Try opening the FAT32 image file in binary mode
    fp = fopen(filename, "rb");
    if (fp == NULL) {  // If the file fails to open, show an error
        printf("Error: Could not find or open the file system image '%s'.\n", filename);
    } else {
        strncpy(Open_Filename, filename, MAX_FILENAME - 1);  // Store the opened filename
        Open_Filename[MAX_FILENAME - 1] = '\0';  // Ensure null termination for safety

        parse_bpb_info();  // Gather BPB (BIOS Parameter Block) data for FAT32
        currentCluster = BPB_RootClus;  // Set the current directory to the root cluster

        printf("Success: File system image '%s' opened and ready.\n", filename);
    }
}



// Close FAT32 file system image
// Function to close the currently open FAT32 file system image
void handle_close() {
    if (fp == NULL) {  // Check if there's an open file system image
        printf("Error: No file system image is currently open.\n");
    } else {
        fclose(fp);  // Close the file using fclose
        fp = NULL;   // Reset the file pointer to indicate no file is open
        printf("Success: File system image closed successfully.\n");
    }
}


// Display BPB information
// Function to display essential BPB (BIOS Parameter Block) information from the FAT32 file system
void handle_info() {
    if (fp == NULL) {  // Check if a file system is open
        printf("There's no file system open.\n");
        return;
    }

    // Display key FAT32 BPB information in both decimal and hexadecimal formats
    printf("Bytes Per Sector (BPB_BytesPerSec): %u (0x%X)\n", BPB_BytesPerSec, BPB_BytesPerSec);
    printf("Sectors Per Cluster (BPB_SecPerClus): %u (0x%X)\n", BPB_SecPerClus, BPB_SecPerClus);
    printf("Reserved Sector Count (BPB_RsvdSecCnt): %u (0x%X)\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
    printf("Number of FATs (BPB_NumFATs): %u (0x%X)\n", BPB_NumFATs, BPB_NumFATs);
    printf("FAT Size (in sectors, BPB_FATSz32): %u (0x%X)\n", BPB_FATSz32, BPB_FATSz32);
    printf("Root Cluster (BPB_RootClus): %u (0x%X)\n", BPB_RootClus, BPB_RootClus);
}


// Parse BPB (BIOS Parameter Block) information from the FAT32 image
void parse_bpb_info() {
    fseek(fp, 11, SEEK_SET);               // BPB_BytesPerSec starts at offset 11
    fread(&BPB_BytesPerSec, 2, 1, fp);

    fseek(fp, 13, SEEK_SET);               // BPB_SecPerClus starts at offset 13
    fread(&BPB_SecPerClus, 1, 1, fp);

    fseek(fp, 14, SEEK_SET);               // BPB_RsvdSecCnt starts at offset 14
    fread(&BPB_RsvdSecCnt, 2, 1, fp);

    fseek(fp, 16, SEEK_SET);               // BPB_NumFATs starts at offset 16
    fread(&BPB_NumFATs, 1, 1, fp);

    fseek(fp, 36, SEEK_SET);               // BPB_FATSz32 starts at offset 36
    fread(&BPB_FATSz32, 4, 1, fp);

    fseek(fp, 44, SEEK_SET);               // BPB_RootClus starts at offset 44
    fread(&BPB_RootClus, 4, 1, fp);

    // Debug output to confirm values
    printf("Parsed BPB Info:\n");
    printf("Bytes per sector: %u\n", BPB_BytesPerSec);
    printf("Sectors per cluster: %u\n", BPB_SecPerClus);
    printf("Reserved sector count: %u\n", BPB_RsvdSecCnt);
    printf("Number of FATs: %u\n", BPB_NumFATs);
    printf("FAT size (sectors): %u\n", BPB_FATSz32);
    printf("Root cluster: %u\n", BPB_RootClus);
}


// Read command to read from a file in the FAT32 image
void handle_read(const char *filename, int position, int num_bytes, const char *option) {
    if (fp == NULL) {
        printf("File system not open.\n");
        return;
    }

    char entry[DIRECTORY_ENTRY_SIZE];
    unsigned int cluster = find_directory_entry(filename, currentCluster, entry);
    if (cluster == 0) {
        printf("File not found.\n");
        return;
    }

    unsigned int fileSize = *(unsigned int *)&entry[28];
    if (position >= fileSize) {
        printf("Position is beyond end of file.\n");
        return;
    }

    // Calculate the sector of the file based on its starting cluster and the position
    unsigned int offsetCluster = cluster + (position / (BPB_BytesPerSec * BPB_SecPerClus));
    unsigned int sectorOffset = ((BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32)) * BPB_BytesPerSec) + (offsetCluster - 2) * BPB_SecPerClus * BPB_BytesPerSec;
    fseek(fp, sectorOffset + (position % (BPB_BytesPerSec * BPB_SecPerClus)), SEEK_SET);

    char *buffer = (char *)malloc(num_bytes);
    fread(buffer, 1, num_bytes, fp);

    // Output the bytes in the specified format
    printf("Data at position %d:\n", position);
    for (int i = 0; i < num_bytes && i + position < fileSize; i++) {
        if (option && strcmp(option, "-ascii") == 0) {
            printf("%c", isprint(buffer[i]) ? buffer[i] : '.');
        } else if (option && strcmp(option, "-dec") == 0) {
            printf("%d ", (unsigned char)buffer[i]);
        } else {
            printf("0x%02X ", (unsigned char)buffer[i]);
        }
    }
    printf("\n");

    free(buffer);
}

// Change directory
// Function to change the current working directory in the FAT32 file system
void handle_cd(const char *dirname) {
    // If the input is ".", stay in the current directory
    if (strcmp(dirname, ".") == 0) {
        return;
    }
    // If the input is "..", move up to the parent directory
    else if (strcmp(dirname, "..") == 0) {
        // Check if we're already in the root directory
        if (currentCluster == BPB_RootClus) {
            printf("Already at the root directory.\n");
            return;
        }

        // Find the parent directory's cluster
        char entry[DIRECTORY_ENTRY_SIZE];
        unsigned int parentCluster = find_directory_entry("..", currentCluster, entry);

        // If no parent directory is found, display an error
        if (parentCluster == 0) {
            printf("Error: Parent directory not found.\n");
            return;
        }

        // Update the current directory to the parent directory
        currentCluster = parentCluster;
        printf("Moved up to the parent directory.\n");
        return;
    }

    // Find the specified directory entry within the current directory
    char entry[DIRECTORY_ENTRY_SIZE];
    unsigned int cluster = find_directory_entry(dirname, currentCluster, entry);

    // If the directory is not found, display an error
    if (cluster == 0) {
        printf("Error: Directory '%s' not found.\n", dirname);
        return;
    }

    // Check if the entry is a directory; if not, display an error
    unsigned char attr = entry[11];
    if (!(attr & 0x10)) {
        printf("Error: '%s' is not a directory.\n", dirname);
        return;
    }

    // Update the current directory to the specified directory
    currentCluster = cluster;
    printf("Changed directory to '%s'.\n", dirname);
}


// List directory contents
void handle_ls() {
    if (fp == NULL) {
        printf("File system not open.\n");
        return;
    }

    readDirectory(currentCluster);  // Read the current directory entries

    for (int i = 0; i < 16; i++) {
        // Skip deleted or empty entries
        if (dir[i].DIR_Name[0] == 0xE5 || dir[i].DIR_Name[0] == 0x00) {
            continue;
        }

        // Skip hidden and system files
        if (dir[i].DIR_Attr & 0x02 || dir[i].DIR_Attr & 0x04) {
            continue;
        }

        // Prepare the name in a printable format (8.3 format)
        char name[12];
        memset(name, 0, 12);           // Initialize name with null characters
        strncpy(name, dir[i].DIR_Name, 11);  // Copy name (8 + 3 format)
        
        // Print directory and file names
        if (dir[i].DIR_Attr & 0x10) {  // Directory attribute
            printf("<DIR> %s\n", name);
        } else {  // Regular file
            printf("%s\n", name);
        }
    }
}






// Handle the stat command to print attributes of a file or directory
void handle_stat(const char *name) {
    if (fp == NULL) {  // Check if the file system is open
        printf("File system not open.\n"); // If not, print an error message
        return; // Exit the function if file system is not open
    }

    char entry[DIRECTORY_ENTRY_SIZE]; // Buffer to store a directory entry
    unsigned int cluster = find_directory_entry(name, currentCluster, entry); // Search for the specified file or directory entry in the current cluster

    if (cluster == 0) {  // If no entry was found
        printf("File not found.\n"); // Print an error message
        return; // Exit the function as there is nothing to display
    }

    unsigned char attr = entry[11]; // Retrieve the attribute byte from the entry (index 11 in FAT32 directory entry structure)
    unsigned short lowCluster = *(unsigned short *)&entry[26]; // Extract the lower 16 bits of the starting cluster number
    unsigned short highCluster = *(unsigned short *)&entry[20]; // Extract the higher 16 bits of the starting cluster number
    unsigned int startCluster = (highCluster << 16) | lowCluster; // Combine high and low cluster values to get the full 32-bit cluster number
    unsigned int fileSize = *(unsigned int *)&entry[28]; // Extract the file size from the entry (stored at offset 28 in the directory entry)

    printf("Attributes: "); // Print the attributes label
    // Check each attribute bit and print corresponding attribute names
    if (attr & 0x01) printf("Read-Only "); // If read-only bit is set, print "Read-Only"
    if (attr & 0x02) printf("Hidden ");    // If hidden bit is set, print "Hidden"
    if (attr & 0x04) printf("System ");    // If system bit is set, print "System"
    if (attr & 0x10) printf("Directory "); // If directory bit is set, print "Directory"
    if (attr & 0x20) printf("Archive ");   // If archive bit is set, print "Archive"
    printf("\n"); // Newline after listing all attributes

    // Print the starting cluster number
    printf("Starting Cluster: %u\n", startCluster);

    // Print file size. If it’s a directory (indicated by attribute bit 0x10), display size as 0.
    printf("Size: %u bytes\n", (attr & 0x10) ? 0 : fileSize);
}


// Put command to add a file to the FAT32 image
void handle_put(const char *localFilename, const char *newFilename) {
    FILE *localFile = fopen(localFilename, "rb");
    if (localFile == NULL) {
        printf("File not found.\n");
        return;
    }

    // Determine the size of the file
    fseek(localFile, 0, SEEK_END);
    long fileSize = ftell(localFile);
    if (fileSize <= 0) {
        printf("Empty or invalid file.\n");
        fclose(localFile);
        return;
    }
    fseek(localFile, 0, SEEK_SET);

    // Read file content into memory
    char *fileContent = (char *)malloc(fileSize);
    if (fileContent == NULL) {
        printf("Memory allocation failed.\n");
        fclose(localFile);
        return;
    }
    fread(fileContent, 1, fileSize, localFile);
    fclose(localFile);

    // Find an empty cluster and directory entry
    unsigned int emptyCluster = find_empty_cluster();
    char entry[DIRECTORY_ENTRY_SIZE];
    unsigned int dirEntryOffset = find_empty_directory_entry(currentCluster, entry);
    if (emptyCluster == 0 || dirEntryOffset == 0) {
        printf("No space available.\n");
        free(fileContent);
        return;
    }

    // Write the file content to the empty cluster
    write_to_cluster(emptyCluster, fileContent, fileSize);
    free(fileContent);

    // Prepare the directory entry for the new file
    memset(entry, ' ', 11);
    strncpy(entry, newFilename, 8);  // Set file name
    entry[11] = 0x20;  // Set attribute to "Archive"
    *(unsigned short *)&entry[26] = emptyCluster & 0xFFFF;
    *(unsigned short *)&entry[20] = (emptyCluster >> 16) & 0xFFFF;
    *(unsigned int *)&entry[28] = fileSize;  // Set file size

    // Write the directory entry to the FAT32 image
    fseek(fp, dirEntryOffset, SEEK_SET);
    fwrite(entry, DIRECTORY_ENTRY_SIZE, 1, fp);

    printf("File '%s' added to FAT32 image.\n", newFilename);
}

// Find an empty cluster in the FAT
unsigned int find_empty_cluster() {
    // Locate an empty cluster in the FAT
    // This is a placeholder implementation
    return 2;  // Example starting cluster
}

// Write data to a specified cluster
void write_to_cluster(unsigned int cluster, const char *data, size_t size) {
    unsigned int sector = ((BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32)) * BPB_BytesPerSec) + (cluster - 2) * BPB_SecPerClus * BPB_BytesPerSec;
    fseek(fp, sector, SEEK_SET);
    fwrite(data, 1, size, fp);
}

// Find an empty directory entry in the specified cluster
unsigned int find_empty_directory_entry(unsigned int cluster, char *entry) {
    unsigned int sector = ((BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32)) * BPB_BytesPerSec) + (cluster - 2) * BPB_SecPerClus * BPB_BytesPerSec;
    fseek(fp, sector, SEEK_SET);

    while (fread(entry, DIRECTORY_ENTRY_SIZE, 1, fp) == 1) {
        if (entry[0] == 0x00 || entry[0] == 0xE5) {
            return ftell(fp) - DIRECTORY_ENTRY_SIZE;
        }
    }

    return 0;  // No empty entry found
}

// Find a directory entry by name and return its cluster
unsigned int find_directory_entry(const char *name, unsigned int cluster, char *entry) {
    unsigned int sector = ((BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz32)) * BPB_BytesPerSec) + (cluster - 2) * BPB_SecPerClus * BPB_BytesPerSec;
    fseek(fp, sector, SEEK_SET);

    char entryName[12];
    while (fread(entry, DIRECTORY_ENTRY_SIZE, 1, fp) == 1) {
        if (entry[0] == 0x00) break;  // End of directory
        if (entry[0] == 0xE5) continue;  // Skip deleted entries

        memset(entryName, 0, 12);
        memcpy(entryName, entry, 8);  // Copy the file name
        entryName[8] = '\0';  // Null-terminate
        for (int i = 7; i >= 0 && entryName[i] == ' '; i--) {
            entryName[i] = '\0';
        }

        if (strcmp(name, entryName) == 0) {
            unsigned short lowCluster = *(unsigned short *)&entry[26];
            unsigned short highCluster = *(unsigned short *)&entry[20];
            return (highCluster << 16) | lowCluster;  // Return the starting cluster
        }
    }

    return 0;  // Not found
}

