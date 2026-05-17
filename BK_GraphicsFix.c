
#include <stdio.h>
#include <unistd.h>

unsigned long long bytes_2_int(unsigned char input[], int len) {
    unsigned long long res = 0;
    for (int i = 0; i < len; i++) {
        res *= 0x100;
        res += (unsigned int)input[i];
    }
    return res;
}
void int_2_bytes(unsigned long long input, int len, unsigned char output[]) {
    unsigned long long tmp;
    for (int i = 0; i < len; i++) {
        tmp = (input & 0xFF);
        output[len - i - 1] = (char) tmp;
        input /= 0x100;
    }
}

void hold() {
    // clear the buffer before calling getchar()
    fseek(stdin, 0, SEEK_END);
    getchar();
}

int main(int argc, char **argv)
{
    printf("=======================================================\n");
    printf("=  BK Graphics Fix v2 by ThatCowGuy / SpaceOmega5000  =\n");
    printf("=======================================================\n\n");

    unsigned char name_buffer[512+1];
    if (argc == 2)
    {
        strcpy(name_buffer, argv[1]);
        printf(" Detected File:\n %s\n", name_buffer);
    }
    else
    {
        printf("[ERROR] - Don't double-click this EXE;\n");
        printf(" Just drag the BIN file onto the EXE file.\n");
        hold();
        return -1;
    }

    int fn_name_len = strlen(name_buffer);
    if (strcmp((name_buffer + fn_name_len - 4), ".bin") != 0) {
        printf("[ERROR] - file doesn't seem to be a BIN file (see file extension).\n");
        hold();
        return -1;
    }
    FILE *fp1 = fopen(name_buffer, "rb");
    if (fp1 == NULL) {
        printf("[ERROR] - unable to open / create specified file (fp1).\n");
        hold();
        return -1;
    }

    int GeoLayout_Offset_Addr = 0x04;
    int DL_Offset_Addr = 0x0C;
    int VL_Offset_Addr = 0x10;
    int CollSetup_Offset_Addr = 0x1C;
	int FX_END_Addr = 0x20;
	int FX_Offset_Addr = 0x24;

    unsigned char buffer[4];
    unsigned long long value_old_B;
    unsigned long long value_old_A;
    unsigned long long value_cur;

    // grab GeoLayout Offset
    fseek(fp1, GeoLayout_Offset_Addr, SEEK_SET);
    fread(buffer, 1, 4, fp1);
    int GeoLayout_Offset = bytes_2_int(buffer, 4);
    printf("\n -- GeoLayout Offset: 0x%08X\n", GeoLayout_Offset);
    // grab DL Offset
    fseek(fp1, DL_Offset_Addr, SEEK_SET);
    fread(buffer, 1, 4, fp1);
    int DL_Offset = bytes_2_int(buffer, 4);
    printf(" -- DL Offset: 0x%08X\n", DL_Offset);
    // grab VL Offset
    fseek(fp1, VL_Offset_Addr, SEEK_SET);
    fread(buffer, 1, 4, fp1);
    int VL_Offset = bytes_2_int(buffer, 4);
    printf(" -- VL Offset: 0x%08X\n", VL_Offset);
    // grab CollSetup Offset
    fseek(fp1, CollSetup_Offset_Addr, SEEK_SET);
    fread(buffer, 1, 4, fp1);
    int CollSetup_Offset = bytes_2_int(buffer, 4);
    printf(" -- CollSetup Offset: 0x%08X\n\n", CollSetup_Offset);
    // grab FX_END Offset
    fseek(fp1, FX_END_Addr, SEEK_SET);
    fread(buffer, 1, 4, fp1);
    int FX_END_Offset = bytes_2_int(buffer, 4);
    printf(" -- FX_END Offset: 0x%08X\n\n", FX_END_Offset);
    // grab FX Offset
    fseek(fp1, FX_Offset_Addr, SEEK_SET);
    fread(buffer, 1, 4, fp1);
    int FX_Offset = bytes_2_int(buffer, 4);
    printf(" -- FX Offset: 0x%08X\n\n", FX_Offset);

	// first of all, we will need to find every instance of a geolayout calling a DL
	int geo_call_addresses[512];
	int geo_call_targets[512];
	int geo_call_shifts[512];
	memset(geo_call_addresses, 0, 512);
	memset(geo_call_targets, 0, 512);
	memset(geo_call_shifts, 0, 512);
	int geo_call_cnt = 0;
	int DL_insertion_cnt = 0;
	unsigned long long GeoCall_Command = 0x00000003;
    fseek(fp1, GeoLayout_Offset, SEEK_SET);
    while (1)
	{
        if (fread(buffer, 1, 4, fp1) < 1)
			break;

        value_cur = bytes_2_int(buffer, 4);
		if (value_cur == GeoCall_Command)
		{
			// we found a DL-calling GeoLayout command; remember it's offset to the GeoLayout start !
			geo_call_addresses[geo_call_cnt++] = (ftell(fp1) - 4);
		}
    }

	for (int i = 0; i < geo_call_cnt; i++)
	{
		printf(" -- GeoLayout Cmd 0x03 found at: 0x%08X\n", geo_call_addresses[i]);
    	fseek(fp1, geo_call_addresses[i], SEEK_SET);
		fread(buffer, 1, 4, fp1);
        value_cur = bytes_2_int(buffer, 4);
		printf("    Reads: 0x%08X\n", value_cur);

		// skip 4 more bytes
		fread(buffer, 1, 4, fp1);
		// read the targetted DL offset aswell now (Note: Its stored as X/8)
		fread(buffer, 1, 2, fp1);
		value_cur = bytes_2_int(buffer, 2);
		geo_call_targets[i] = value_cur * 0x08;
	}
	printf("\n");
	
	// now we can run through the file again, but we start inserting BA commands infront of B8s
    // scan for the end of DL (upper half should suffice)
    unsigned long long DL_fix_command = 0xBA000E02;
    unsigned long long DL_end_command = 0xB8000000;
	
	// in this loop, we only want to see which geo-calls need to be shifted by how much
	fseek(fp1, 0, SEEK_SET);
    while (fread(buffer, 1, 4, fp1) > 0)
	{
        value_cur = bytes_2_int(buffer, 4);
		if (value_cur == DL_end_command)
        {
			printf(" -- DL_End Cmd 0xB8 found at: 0x%08X absolute (0x%08X relative to real DL-start)\n",
				(ftell(fp1) - 4),
				(ftell(fp1) - 4 - (DL_Offset + 0x08))
			);

			// check if this DL_end is preceeded by a BA already
			if (value_old_B == DL_fix_command)
			{
				printf("    already preceeded by an 0xBA; Ignoring.\n");
			}
			else
			{
				printf("    missing preceeding 0xBA; Inserting later.\n");
				// determine which geo-calls need shifting
				for (int i = 0; i < geo_call_cnt; i++)
				{
					// if the geo-call target is greater than the relative offset of this B8 to the start of the DL-Seg,
					// we need to shift it by 1 additional step later on
					if (geo_call_targets[i] >= (ftell(fp1) - 4 - (DL_Offset + 0x08)))
					{
						geo_call_shifts[i] += 1;
					}
				}
				DL_insertion_cnt += 1;
			}
        }
		// remember the last 2 read values for BA-checks (see above)
		value_old_B = value_old_A;
		value_old_A = value_cur;
    }
	for (int i = 0; i < geo_call_cnt; i++)
	{
		printf(" -- Listing GeoLayout 0x03-Call Adjustments:\n");
		printf("    -- Call @ 0x%08X (Targetting relative 0x%08X) will be shifted by 0x%04X\n",
			geo_call_addresses[i],
			geo_call_targets[i],
			geo_call_shifts[i] * 8
		);
	}
	printf("\n");
	
    // adjust the affected offsets
	int offset_shift = DL_insertion_cnt * 8;
	printf(" -- shifting all header-based Segment offsets by 0x%04X.\n", offset_shift);

	// open the new file
    name_buffer[fn_name_len - 4] = '\0'; // remove ".bin"
    strcat(name_buffer, "_fixed.bin");
    printf(" Start Writing to File:\n %s\n\n", name_buffer);
    FILE *fp2 = fopen(name_buffer, "wb");
    if (fp2 == NULL) {
        printf("[ERROR] - unable to open / create specified file (fp2, wb mode).\n");
        hold();
        return -1;
    }
	// this is the loop in which we (over)write fp2
	fseek(fp1, 0, SEEK_SET);
	fseek(fp2, 0, SEEK_SET);
    while (fread(buffer, 1, 4, fp1) > 0)
	{
        value_cur = bytes_2_int(buffer, 4);

		// if we are looking at one of the header offsets, just overwrite it,
		// and "continue" to avoid the other write at the bottom here
		if (
			ftell(fp1) - 4 == GeoLayout_Offset_Addr ||
			ftell(fp1) - 4 == VL_Offset_Addr ||
			ftell(fp1) - 4 == CollSetup_Offset_Addr ||
			ftell(fp1) - 4 == FX_END_Addr ||
			ftell(fp1) - 4 == FX_Offset_Addr
		)
		{
			// mind the edgecase, that these segments might not exist
			if (value_cur == 0x00)
			{
				int_2_bytes(0x00, 4, buffer);
				printf("    -- writing 0x%08X to 0x%08X (header offset)\n", 0x00, ftell(fp2));
				fwrite(buffer, 1, 4, fp2);
				continue;
			}
			int_2_bytes((value_cur + offset_shift), 4, buffer);
			printf("    -- writing 0x%08X to 0x%08X (header offset)\n", (value_cur + offset_shift), ftell(fp2));
        	fwrite(buffer, 1, 4, fp2);
			continue;
		}
		
		if (value_cur == DL_end_command)
        {
			// check if this DL_end is preceeded by a BA already
			if (value_old_B == DL_fix_command)
			{ }
			else
			{
				int_2_bytes(DL_fix_command, 4, buffer);
        		fwrite(buffer, 1, 4, fp2);
				int_2_bytes(0x00000000, 4, buffer);
        		fwrite(buffer, 1, 4, fp2);
			}
        }
		// remember the last 2 read values for BA-checks (see above)
		value_old_B = value_old_A;
		value_old_A = value_cur;

		// also catch when we enter the DL segment, so we can adjust the DL-command cnt there
		if (ftell(fp1) - 4 == DL_Offset)
		{	
			// increment the cnt by how many instructions we inserted (or figured out we have to insert)
			printf("    -- incrementing DL-cmd-cnt from 0x%04X to 0x%04X (DL-Seg start @ 0x%08X)\n", value_cur, (value_cur + DL_insertion_cnt), ftell(fp2));
			int_2_bytes((value_cur + DL_insertion_cnt), 4, buffer);
			fwrite(buffer, 1, 4, fp2);
			continue;
		}

		// check if fp1 is at one of the geolayout calls
		int overwriting_Geo = 0;
		for (int i = 0; i < geo_call_cnt; i++)
		{
			if (ftell(fp1) - 4 == geo_call_addresses[i])
			{
				// write the 0x03 portion
        		fwrite(buffer, 1, 4, fp2);

				// also write the 4 bytes that denote the geo-cmd-chain length
				fread(buffer, 1, 4, fp1);
        		fwrite(buffer, 1, 4, fp2);

				// get the next 2 bytes, and add the corresponding shift onto that
				fread(buffer, 1, 2, fp1);
        		int target = bytes_2_int(buffer, 2);
				target += geo_call_shifts[i];
				int_2_bytes(target, 2, buffer);
				// rewrite that to fp2
				printf("    -- writing 0x%04X to 0x%08X (geo-call)\n", (target * 8), ftell(fp2));
        		fwrite(buffer, 1, 2, fp2);
				// and get the next 2 bytes from fp1 unchanged
				fread(buffer, 1, 2, fp1);
        		fwrite(buffer, 1, 2, fp2);

				overwriting_Geo = 1;
				break;
			}
		}
		if (overwriting_Geo == 1) continue;
		
		// in all other cases, we just copy the content over to fp2
		// and dont forget to restore the buffer you buffoon (heh) !
		int_2_bytes(value_cur, 4, buffer);
        fwrite(buffer, 1, 4, fp2);
    }
    fclose(fp1);
    fclose(fp2);
	printf("\n");

    printf(" -- adjusted affected GeoLayout DL-cmd offsets.\n\n");
    printf(" -- DONE! (you can close this window now).\n");
    hold();
}