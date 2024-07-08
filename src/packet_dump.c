#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <net/packet.h>

void write_packet_to_file(char* dir, char* filename, struct btide_packet* pkt) {
    char file[4096];
    snprintf(file, sizeof(file), "%s/%s", dir, filename);
    FILE* file_ptr = fopen(file, "wb");
    if (file_ptr == NULL) {
        exit(1);
    }
    fwrite(pkt, sizeof(struct btide_packet), 1, file_ptr);
    fclose(file_ptr);
}

int main(int argc, char** argv) {

    char* dir = argv[1];

    struct stat sb;
    if (stat(dir, &sb) == 0) {
        if (S_ISDIR(sb.st_mode)) {
            ;
        }
        else {
            // It is a file
            perror("Directory provided is a file");
            return 1;
        }
    }
    else {
        perror("Directory does not exist");
        return 1;
    }

    // Generate and write packets to directory
    struct btide_packet ACP = generate_ACP();
    struct btide_packet ACK = generate_ACK();
    struct btide_packet DSN = generate_DSN();
    struct btide_packet PNG = generate_PNG();
    struct btide_packet POG = generate_POG();

    // Sample data
    char ident[1024] = "0123456789abcdef0123456789abcdef0123456789abcdef0123457"
                    "0123456789abcdef0123456789abcdef0123456789abcdef012345678f"
                    "0123456789abcdef0123456789abcdef0123456789abcdef012345678d"
                    "0123456789abcdef0123456789abcdef0123456789abcdef012345678f"
                    "0123456789abcdef0123456789abcdef0123456789abcdef0123456784"
                    "0123456789abcdef0123456789abcdef0123456789abcdef012345678f"
                    "0123456789abcdef0123456789abcdef0123456789abcdef012345678d"
                    "0123456789abcdef0123456789abcdef0123456789abcdef012345678d"
                    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789"
                    "0123456789abcdef0123456789abcdef0123456789abcdef012345678d"
                    "0123456789abcdef0123456789abcdef0123456789";
    char hash[64] = "39306a85bc0da61449840702883e7a29fb227980a7aeef7688b48401ed"
                    "21b9a";
    uint32_t data_len = 1024;
    uint32_t offset = 0;

    struct btide_packet REQ = generate_REQ(ident, hash, data_len, offset);

    write_packet_to_file(dir, "acp.pkt", &ACP);
    write_packet_to_file(dir, "ack.pkt", &ACK);
    write_packet_to_file(dir, "dsn.pkt", &DSN);
    write_packet_to_file(dir, "png.pkt", &PNG);
    write_packet_to_file(dir, "pog.pkt", &POG);
    write_packet_to_file(dir, "req.pkt", &REQ);

    return 0;
}

