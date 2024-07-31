#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <utility>

using std::pair;
using std::map;
using std::make_pair;

typedef pair < pair < uint32_t, uint32_t >, uint32_t > file_id;
map < file_id, uint32_t > file_map;

constexpr file_id make_file_id (uint32_t dev_major, uint32_t dev_minor, uint32_t inum) {
  return make_pair (make_pair (dev_major, dev_minor), inum);
}

uint32_t read_4_octets (const uint8_t *input) {
  uint32_t result = 0;
  for (uint32_t i = 0; i < 8; ++i) {
    uint8_t next_digit = input[i];
    if (next_digit >= '0' && next_digit <= '9') {
      next_digit -= '0';
    } else if (next_digit >= 'A' && next_digit <= 'F') {
      next_digit -= 'A';
      next_digit += 10;
    }
    result = result * 16 + next_digit;
  }
  return result;
}

void write_4_octets (uint8_t *output, uint32_t num) {
  for (uint32_t i = 0; i < 8; ++i) {
    uint8_t next_digit = num % 16;
    if (next_digit < 10) {
      next_digit += '0';
    } else {
      next_digit -= 10;
      next_digit += 'A';
    }
    output[7 - i] = next_digit;
    num /= 16;
  }
}

int main (int argc, char *argv[]) {
  if (argc != 2) {
    printf ("Usage: patch_cpio [cpio_file]\n");
    return 0;
  }

  FILE *in_fd = fopen (argv[1], "r+");
  if (in_fd == NULL) {
    fprintf (stderr, "Error: Unable to open input file\n");
    return 1;
  }

  uint32_t curr_inum = 2; // Start from 2 since 1 is for bad block

  do {
    printf ("Processing next record, current offset %ld\n", ftell (in_fd));
    /* Read header of next record */
    uint8_t header[110];
    size_t ret = fread (header, 1, 110, in_fd);
    if (ret < 110) {
      fprintf (stderr, "Reading header failed\n");
      return 1;
    }

    /* Assign inode number */
    uint32_t major = read_4_octets (header + 62);
    uint32_t minor = read_4_octets (header + 70);
    uint32_t inum = read_4_octets (header + 6);

    if (inum == 0) {
      /* Check whether this is the trailer or not */
      uint32_t path_len = read_4_octets (header + 94);
      if (path_len != 11) {
	printf ("Error: Encountered record with inode num = 0 but not TRAILER!!! file\n");
	return 1;
      }
      uint8_t path[11] = {0};
      ret = fread (path, 1, 11, in_fd);
      if (ret != 11) {
	fprintf (stderr, "Attempted to check TRAILER!!! path but failed\n");
	return 1;
      }
      if (strncmp ((char *) path, "TRAILER!!!", 11) != 0) {
	printf ("Error: Encountered record with inode num = 0 but not TRAILER!!! file\n");
	return 1;
      }
      printf ("Encountered TRAILER!!! record, terminating\n");
      return 0;
    }

    if (auto iter = file_map.find (make_file_id (major, minor, inum)); iter != file_map.end ()) {
      write_4_octets (header + 6, iter->second);
    } else {
      file_map[make_file_id (major, minor, inum)] = curr_inum;
      write_4_octets (header + 6, curr_inum);
      curr_inum++;
    }

    /* Set permission */
    uint32_t perm = read_4_octets (header + 14);
    perm |= 0600;
    perm &= ~ 07077;
    write_4_octets (header + 14, perm);

    /* Set uid, gid to 0 */
    write_4_octets (header + 22, 0);
    write_4_octets (header + 30, 0);

    /* Erase mtime */
    /* 946684800 is Jan 01 2000 00:00:00 GMT+0000 */
    write_4_octets (header + 46, 946684800);

    /* Erase device number */
    /* Block device (1, 0) is /dev/ram0 */
    write_4_octets (header + 62, 1);
    write_4_octets (header + 70, 0);

    /* Write patched header */
    ret = fseek (in_fd, -110, SEEK_CUR);
    if (ret) {
      fprintf (stderr, "fseek failed\n");
      return 1;
    }

    ret = fwrite (header, 1, 110, in_fd);
    if (ret != 110) {
      fprintf (stderr, "fwrite failed\n");
      return 1;
    }

    /* Seek to next header */
    uint32_t path_len = read_4_octets (header + 94);
    printf ("Length of file path is %u\n", path_len);
    size_t offset = 110 + path_len;
    offset += (4 - offset % 4) % 4;
    uint32_t file_len = read_4_octets (header + 54);
    printf ("Length of file is %u\n", file_len);
    offset += file_len;
    offset += (4 - offset % 4) % 4;
    offset -= 110;

    printf ("Finished patching, skipping %lu bytes\n", offset);
    ret = fseek (in_fd, offset, SEEK_CUR);
    if (ret) {
      fprintf (stderr, "fseek failed\n");
      return 1;
    }
  } while (1);

  return 0;
}
