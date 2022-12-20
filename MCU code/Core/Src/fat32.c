#include <stddef.h>
#include "sdcard.h"
#include "fat32.h"
#include "main.h"

/*
 * Magic constants to use when working with FAT32 filesystem
 */
#define MAX_PRIMARY_PARTITIONS				(4)
#define PARTITION_TABLE_FIRST_ENTRY_OFFSET	(0x1BE)
#define PARTITION_TYPE_WIN95_OSR2_FAT32		(0xB)
#define PARTITION_TYPE_WIN95_OSR2_FAT32_LBA	(0xC)
#define FAT32_EBPB_SIGNATURE1				(0x28)
#define FAT32_EBPB_SIGNATURE2				(0x29)
#define FAT32_BOOT_PARTITION_SIGNATURE		(0xAA55)
#define UNUSED_DIR_ENTRY_NAME_FIRST_BYTE	(0xE5)
#define EXPECTED_FAT_COUNT					(2)

/*
 * File attribute values from https://wiki.osdev.org/FAT32#Standard_8.3_format
 */
#define FAT32_FILE_ATTR_RD_ONLY         (1 << 0)
#define FAT32_FILE_ATTR_HIDDEN          (1 << 1)
#define FAT32_FILE_ATTR_SYSTEM          (1 << 2)
#define FAT32_FILE_ATTR_VOL_ID          (1 << 3)
#define FAT32_FILE_ATTR_DIR             (1 << 4)
#define FAT32_FILE_ATTR_ARCHIVE         (1 << 5)
#define FAT32_FILE_ATTR_LONG_FILENAME   (0xF)

/*
 * Internal data structures to use when working with FAT32 filesystem
 */
static uint32_t fat_begin_lba;
static uint32_t cluster_begin_lba;
// We can skip this since we check that it should be a fixed value during
// initialization
// static uint32_t sectors_per_cluster;
static uint32_t root_dir_first_cluster;
static uint8_t cluster_cache[SECTORS_PER_CLUSTER * SECTOR_SIZE];

/**
 * Data structure to access partition table entry fields
 */
typedef struct {
    uint8_t     boot_indicator;
    uint8_t     starting_head;
    uint16_t    starting_sector_and_cylinder;
    uint8_t     system_id;
    uint8_t     ending_head;
    uint16_t    ending_sector_and_cylinder;
    uint32_t    starting_lba;
    uint32_t    total_sectors;
} Partition_Table_Entry;

/**
 * Data structure to access FAT32 Extended Boot Record fields
 */
typedef struct {
    uint32_t    sectors_per_fat;
    uint16_t    flags;
    uint8_t     fat_version[2];
    uint32_t    root_dir_cluster;
    uint16_t    fsinfo_sector;
    uint16_t    backup_boot_sector;
    uint8_t     __reserved[12];
    uint8_t     drive_number;
    uint8_t     __reserved2_or_flags;   // Flags only for Windows NT
    uint8_t     signature;
    uint32_t    volume_id;
    uint8_t     volume_label[11];
    uint8_t     system_identifier[8];   // do not trust
    uint8_t     boot_code[420];
    uint16_t    bootable_partition_signature;
} __attribute__((packed)) FAT32_EB_Record;

/**
 * Data structure to access BIOS Parameter block
 */
typedef struct {
    uint8_t     jmp[3];
    uint8_t     oem_identifier[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    reserved_sectors;
    uint8_t     number_of_fat;
    uint16_t    number_of_root_dir_entries;
    uint16_t    total_sectors_count;
    uint8_t     media_descriptor_type;
    uint16_t    sectors_per_fat;    // for fat12/16 only
    uint16_t    sectors_per_track;  // do not trust
    uint16_t    heads_count;    // do not trust
    uint32_t    hidden_sectors_count;
    uint32_t    large_sectors_count;
    FAT32_EB_Record   ebpb_rec;
} __attribute__((packed)) BPB_Record;

/**
 * Data structure to access fields of directory/file entries with standard 8.3
 * format
 */
typedef struct {
    uint8_t     filename_8_3[11];
    uint8_t     file_attrs;
    uint8_t     __reserved; // For use by Win NT
    uint8_t     creation_time_tenth_secs;
    uint16_t    creation_time;
    uint16_t    creation_date;
    uint16_t    last_accessed_date;
    uint16_t    first_cluster_high;
    uint16_t    last_modification_time;
    uint16_t    last_modification_date;
    uint16_t    first_cluster_low;
    uint32_t    file_size;
} __attribute__((packed)) DIR_8_3_Record;

static uint32_t Get_Lowest_Partition_LBA(void);
static void Get_File_Begin_Cluster_And_Size(
	const char *restrict const filename,
	uint32_t *restrict const file_begin_cluster,
	uint32_t *restrict const file_size);
static Boolean Filenames_Match(
		const char *restrict const fat32_direntry_filename,
		const char *restrict const filename);
static inline uint32_t Min(const uint32_t a, const uint32_t b);
static inline char to_upper(char c);

FAT32_Status FAT32_Init(void) {
	const uint32_t lowest_partition_lba = Get_Lowest_Partition_LBA();
	if (lowest_partition_lba == 0) {
		return FAT32_INIT_PARTITION_DISCOVERY_ERR;
	}

	if (SDC_Read_Sector(lowest_partition_lba, cluster_cache) != SDC_OK) {
		return FAT32_INIT_PARTITION_READ_ERR;
	}

	const BPB_Record *restrict const bpb_record = (BPB_Record *) cluster_cache;
	if (bpb_record->bytes_per_sector != SECTOR_SIZE) {
		return FAT32_INIT_UNSUPPORTED_SECTOR_SIZE;
	}
	if (bpb_record->number_of_fat != EXPECTED_FAT_COUNT) {
		return FAT32_INIT_UNSUPPORTED_FAT_COUNT;
	}
	if ((bpb_record->ebpb_rec.signature != FAT32_EBPB_SIGNATURE1) &&
			(bpb_record->ebpb_rec.signature != FAT32_EBPB_SIGNATURE2)) {
		return FAT32_INIT_INVALID_EBPB_SIGNATURE;
	}
	if (bpb_record->ebpb_rec.bootable_partition_signature !=
			FAT32_BOOT_PARTITION_SIGNATURE) {
		return FAT32_INIT_INVALID_BOOT_PARTITION_SIGNATURE;
	}
	if (bpb_record->sectors_per_cluster != SECTORS_PER_CLUSTER) {
		return FAT32_INIT_UNSUPPORTED_SECTORS_PER_CLUSTER;
	}

	fat_begin_lba = lowest_partition_lba + bpb_record->reserved_sectors;
	cluster_begin_lba = fat_begin_lba +
			(bpb_record->number_of_fat * bpb_record->ebpb_rec.sectors_per_fat);
	//sectors_per_cluster = bpb_record->sectors_per_cluster;
	root_dir_first_cluster = bpb_record->ebpb_rec.root_dir_cluster;

	return FAT32_OK;
}

FAT32_Status FAT32_Read_File_From_Root_Dir_And_Process_Data(
		const char *restrict const filename,
		uint8_t *restrict const buffer,
		DataBufferProcessingCallback cb) {
	uint32_t file_begin_cluster;
	uint32_t current_cluster;
	uint32_t current_lba;
	uint32_t file_cluster_index = 0;
	uint32_t file_size;
	uint32_t data_size;

	// Find where the file starts and how long is it
	Get_File_Begin_Cluster_And_Size(filename, &file_begin_cluster, &file_size);
	if (file_begin_cluster == 0) {
		return FAT32_READ_FILE_NOT_FOUND;
	}

	// Start reading the file from the start position
	current_cluster = file_begin_cluster;
	do {
		// Read the current cluster into user provided buffer
		current_lba = cluster_begin_lba + (current_cluster - 2) * SECTORS_PER_CLUSTER;
		if (SDC_Read_Sector(current_lba, buffer) != SDC_OK) {
			return FAT32_READ_FILE_ERR;
		}
		if (SDC_Read_Sector(current_lba + 1, buffer + SECTOR_SIZE) != SDC_OK) {
			return FAT32_READ_FILE_ERR;
		}

		// Process 1 cluster worth of data using user provided callback
		data_size = Min(SECTORS_PER_CLUSTER * SECTOR_SIZE, file_size);
		if ((*cb)(file_cluster_index * SECTORS_PER_CLUSTER * SECTOR_SIZE, buffer,
				data_size) != DATA_PROCESSING_OK) {
			return FAT32_READ_FILE_DATA_PROCESS_ERR;
		}

		// Update the length of data left to be read and calculate the next
		// cluster to read
		file_size -= data_size;
		current_cluster &= 0x0FFFFFFF;
		if (SDC_Read_Sector(fat_begin_lba + (current_cluster >> 7),
				cluster_cache) != SDC_OK) {
			return FAT32_READ_FAT_READ_ERR;
		}
		current_cluster = *((uint32_t *)cluster_cache + (current_cluster & 0x7F));

		// Change offset to calculate data offset for next call to callback
		// function
		file_cluster_index++;
	} while (current_cluster != 0x0FFFFFFF);

	return FAT32_OK;
}

/**
 * Get logical block address for the partition that has the lowest value for LBA
 *
 * @return	LBA for partition having lowest LBA value. 0 indicated error.
 */
static uint32_t Get_Lowest_Partition_LBA(void) {
	uint32_t lowest_partition_lba = 0;
	if (SDC_Read_Sector(0, cluster_cache) != SDC_OK) {
		return 0;
	}

	// Iterate over all primary partition entries
	Partition_Table_Entry *pentry = (Partition_Table_Entry *) (cluster_cache +
															   PARTITION_TABLE_FIRST_ENTRY_OFFSET);
	for (uint8_t i = 0; i < MAX_PRIMARY_PARTITIONS; i++) {
		if ((pentry->system_id == PARTITION_TYPE_WIN95_OSR2_FAT32) ||
				(pentry->system_id == PARTITION_TYPE_WIN95_OSR2_FAT32_LBA)) {
			if ((lowest_partition_lba == 0) ||
					(pentry->starting_lba < lowest_partition_lba)) {
				lowest_partition_lba = pentry->starting_lba;
			}
		}
		pentry++;
	}
	return lowest_partition_lba;
}

/**
 * Find the user provided filename. If file exists, update variables to store
 * the cluster where the file's data begins and the size of file
 *
 * @param filename				(IN)	Name of file to find
 * @param file_begin_cluster	(OUT)	Variable to store cluster value where the
 * 										file data starts from. If file is not found,
 * 										this will be 0
 * @param file_size				(OUT)	Variable to store number of bytes in the file
 */
static void Get_File_Begin_Cluster_And_Size(
		const char *restrict const filename,
		uint32_t *restrict const file_begin_cluster,
		uint32_t *restrict const file_size) {

	uint32_t current_cluster = root_dir_first_cluster;
	uint32_t current_lba;
	const DIR_8_3_Record *restrict dir_record = NULL;

	// Don't expect pre-initialized values
	*file_begin_cluster = 0;
	*file_size = 0;

	do {
		// Read the cluster containing the directory/file entries
		current_lba = cluster_begin_lba + (current_cluster - 2) * SECTORS_PER_CLUSTER;
		if (SDC_Read_Sector(current_lba, cluster_cache) != SDC_OK) {
			return;
		}
		if (SDC_Read_Sector(current_lba+1, cluster_cache + SECTOR_SIZE) != SDC_OK) {
			return;
		}

		// Iterate through all the directory/file entries and find the one
		// matching the given filename
		dir_record = (DIR_8_3_Record *) cluster_cache;
		while ((((uint8_t *)dir_record - cluster_cache) < sizeof(cluster_cache)) &&
				(dir_record->filename_8_3[0] != '\0')) {
			if ((dir_record->filename_8_3[0] != UNUSED_DIR_ENTRY_NAME_FIRST_BYTE) &&
					(dir_record->file_attrs != FAT32_FILE_ATTR_LONG_FILENAME) &&
					(dir_record->file_attrs != FAT32_FILE_ATTR_VOL_ID) &&
					(dir_record->file_attrs != FAT32_FILE_ATTR_SYSTEM) &&
					(dir_record->file_attrs != FAT32_FILE_ATTR_DIR)) {
				if (Filenames_Match((const char *)dir_record->filename_8_3,
						filename) == TRUE) {
					// If we found the file, update the provided variables and
					// return
					*file_begin_cluster = ((dir_record->first_cluster_high << 16) |
							(dir_record->first_cluster_low));
					*file_size = dir_record->file_size;
					return;
				}
			}
			dir_record++;
		}

		// Compute the next cluster to read
		current_cluster &= 0x0FFFFFFF;
		if (SDC_Read_Sector(fat_begin_lba + (current_cluster >> 7),
				cluster_cache) != SDC_OK) {
			return;
		}
		current_cluster = *((uint32_t *)cluster_cache + (current_cluster & 0x7F));
	} while (current_cluster != 0x0FFFFFFF);
	return;
}

/**
 * Check if 2 filenames match each other or not
 *
 * @param fat32_direntry_filename	(IN)	Filename from FAT32 directory entry
 * @param filename					(IN)	Filename provided by the user
 *
 * @return	True if the filenames match with a custom criteria. False otherwise.
 */
static Boolean Filenames_Match(const char *restrict const fat32_direntry_filename,
							   const char *restrict const filename) {
	uint8_t i = 0;;
	uint8_t j = 0;

	while ((i < 11) && (filename[j] != '\0')) {
		if (to_upper(filename[j]) == to_upper(fat32_direntry_filename[i])) {
			i++;
			j++;
		}
		else if (filename[j] == '.') {
			// Skips '.' in user provided filename. So there can be no '.' or
			// multiple '.' in user provided filename, and it will still match.
			// We can impose extra condition by checking that exactly one '.'
			// character is present in the user provided filename if we would
			// like to
			j++;
		}
		else if (fat32_direntry_filename[i] == ' ') {
			i++;
		}
		else {
			return FALSE;
		}
	}
	if ((filename[j] == '\0') && (i == 11)) {
		return TRUE;
	}
	return FALSE;
}

/**
 * Helper function to convert lowercase character to uppercase character
 *
 * @param c	(IN)	Character to convert to uppercase
 *
 * @return	Uppercase character if input is a lowercase character. Otherwise the
 * 			input character is returned as it is.
 */
static inline char to_upper(const char c) {
	if ((c >= 'a') && (c <= 'z')) {
		return (c + ('A' - 'a'));
	}
	return c;
}

/**
 * Helper function to compare two integers and find the minimum one
 *
 * @param a	(IN)	Integer to compare
 * @param b	(IN)	Integer to compare
 *
 * @return	Minimum value out of the provided integers
 */
static inline uint32_t Min(const uint32_t a, const uint32_t b) {
	if (a < b) {
		return a;
	}
	return b;
}
