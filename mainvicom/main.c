#include <stdio.h>
#include <linux/fb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <poll.h>
#include <linux/input.h>
#include <time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>



/*
FFMPEG VIDEO RX:

ffmpeg -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -f mpegts -i udp://:8000?listen=1 -fflags nobuffer -flags low_delay -pix_fmt rgb555le -f fbdev /dev/fb0

FFMPEG VIDEO TX:

ffmpeg -re -i /home/indir/Загрузки/video_2023-07-29_20-45-50.mp4 -r 6 -vcodec mpeg2video -filter:v "scale=640x480" -bufsize 128k -maxrate 600k -fflags nobuffer -flags low_delay -avioflags direct -f mpegts -an udp://192.168.11.70:8000?pkt_size=1024

FFMPEG AUDIO RX:

ffmpeg -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -f mpegts -i udp://:8000?listen=1 -fflags nobuffer -flags low_delay -f s16le -acodec pcm_s16le - | aplay --period-size=2048 --buffer-size=8192 -f S16_LE -r 16000 -

FFMPEG AUDIO TX:

ffmpeg -re -i /home/indir/Загрузки/video_2023-07-29_20-45-50.mp4 -r 6 -vcodec mpeg2video -filter:v "scale=640x480" -acodec mp3 -ac 1 -ar 16000 -filter:a "volume=0.01" -bufsize 128k -maxrate 600k -fflags nobuffer -flags low_delay -avioflags direct -f mpegts -vn udp://192.168.11.70:8000?pkt_size=1024

FFMPEG REVERSE AUDIO RX:

ffmpeg -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -re -f s16le -ac 1 -ar 16000 -avioflags direct -i udp://:8000?listen=1 -fflags nobuffer -flags low_delay -avioflags direct -f s16le -acodec pcm_s16le - | aplay -f S16_LE -r 16000 -c 1 -

FFMPEG REVERSE AUDIO TX:

arecord -c 1 -f S16_LE -r 16000 -t raw | ffmpeg -f s16le -acodec pcm_s16le -ac 1 -ar 16000 -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -avioflags direct -i pipe: -c:a copy -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -avioflags direct -f s16le udp://192.168.11.3:8000?pkt_size=1024



ffmpeg -re -i /home/indir/Загрузки/video_2023-07-29_20-45-50.mp4 -r 6 -vcodec mpeg2video -filter:v "scale=640x480" -bufsize 128k -maxrate 600k -acodec mp3 -ac 1 -ar 16000 -filter:a "volume=0.01" -fflags nobuffer -flags low_delay -avioflags direct -f mpegts udp://192.168.11.76:8101?pkt_size=1024

ffmpeg -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -re -f s16le -ac 1 -ar 16000 -avioflags direct -i udp://:8102?listen=1 -fflags nobuffer -flags low_delay -avioflags direct -f s16le -acodec pcm_s16le - | aplay -f S16_LE -r 16000 -c 1 -

*/

#define BMP_DATA_OFFSET_OFFSET 0x000A
#define BMP_WIDTH_OFFSET 0x0012
#define BMP_HEIGHT_OFFSET 0x0016
#define BMP_BITS_PER_PIXEL_OFFSET 0x001C
#define BMP_HEADER_SIZE 14
#define BMP_INFO_HEADER_SIZE 40
#define BMP_NO_COMPRESION 0
#define BMP_MAX_NUMBER_OF_COLORS 0
#define BMP_ALL_COLORS_REQUIRED 0

#define BGCOLOR 0b0001000010011001

#define CLOCK_CORR_COEFF 20

enum modes {
    MODE_IDLE,
    MODE_IDLE_SCROFF,
    MODE_NO_CONNECTION,
    MODE_NO_INTERNET,
    MODE_MANUAL_ACTIVE,
    MODE_IDLE_REQUEST,
    MODE_REQUEST_ACTIVE,
    MODE_REQUEST_RESPONDING
};

enum modes curr_mode = MODE_IDLE_SCROFF;

int fb_fd = -1;
int fb_width;
int fb_height;
int fb_bpp;
int fb_bytes;
int fb_data_size;
char *fb_data;
char fb_powercfg_path[256] = {0};

int kbd_fd = -1;

uint8_t* logo_bmp_data;
uint32_t logo_bmp_width;
uint32_t logo_bmp_height;

uint8_t* nocon_bmp_data;
uint32_t nocon_bmp_width;
uint32_t nocon_bmp_height;

uint8_t* nonet_bmp_data;
uint32_t nonet_bmp_width;
uint32_t nonet_bmp_height;

int sock_fd = -1;

int a_pid = -1;
int v_pid = -1;

struct timeval timeval_subtract (struct timeval x, struct timeval y) {
    struct timeval result;
    /* Perform the carry for the later subtraction by updating y. */
    if (x.tv_usec < y.tv_usec) {
        int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
        y.tv_usec -= 1000000 * nsec;
        y.tv_sec += nsec;
    }
    if (x.tv_usec - y.tv_usec > 1000000) {
        int nsec = (y.tv_usec - x.tv_usec) / 1000000;
        y.tv_usec += 1000000 * nsec;
        y.tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
        tv_usec is certainly positive. */
    result.tv_sec = x.tv_sec - y.tv_sec;
    result.tv_usec = x.tv_usec - y.tv_usec;

    return result;
}

void read_bmp(const char *fileName, uint8_t **pixels, uint32_t *width, uint32_t *height) {
        FILE *imageFile;
        uint32_t dataOffset;
        uint32_t bytesPerPixel;
        uint16_t bitsPerPixel;
        int paddedRowSize;
        int unpaddedRowSize;
        int totalSize;
        uint8_t *currentRowPointer;

        imageFile = fopen(fileName, "rb");

        fseek(imageFile, BMP_DATA_OFFSET_OFFSET, SEEK_SET);
        fread(&dataOffset, 4, 1, imageFile);
        fseek(imageFile, BMP_WIDTH_OFFSET, SEEK_SET);
        fread(width, 4, 1, imageFile);
        fseek(imageFile, BMP_HEIGHT_OFFSET, SEEK_SET);
        fread(height, 4, 1, imageFile);
        fseek(imageFile, BMP_BITS_PER_PIXEL_OFFSET, SEEK_SET);
        fread(&bitsPerPixel, 2, 1, imageFile);
        bytesPerPixel = ((uint32_t)bitsPerPixel) / 8;

        if(bytesPerPixel != fb_bytes) {
            printf("BMP WRONG BPP: %s(%d/%d)\n", fileName, bytesPerPixel, fb_bytes);
            *pixels = 0;
            *width = 0;
            *height = 0;
            return;
        }

        paddedRowSize = (int)(4 * ceil((float)(*width) / 4.0f))*(bytesPerPixel);
        unpaddedRowSize = (*width)*bytesPerPixel;
        totalSize = unpaddedRowSize*(*height);
        *pixels = (uint8_t*)malloc(totalSize);
        currentRowPointer = *pixels+((*height-1)*unpaddedRowSize);
        for (int i = 0; i < *height; i++) {
            fseek(imageFile, dataOffset+(i*paddedRowSize), SEEK_SET);
            fread(currentRowPointer, 1, unpaddedRowSize, imageFile);
            currentRowPointer -= unpaddedRowSize;
        }

        fclose(imageFile);
}

void fb_display_bmp(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
    // printf("Displaying BMP(%dx%d) on %d:%d\n", width, height, x, y);
    char* linestart;
    for(int i = 0; i < height; i++) {
        linestart = fb_data + (((y+i)*fb_width + x)*fb_bytes);
        memcpy(linestart, pixels + (i*width)*fb_bytes, width*fb_bytes);
    }
}

void fb_fill(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint16_t color) {
    char* linestart;
    for(int i = 0; i < height; i++) {
        linestart = fb_data + (((y+i)*fb_width + x)*fb_bytes);
        // memcpy(linestart, pixels + (i*width)*fb_bytes, width*fb_bytes);
        for(int k = 0; k < width; k++) {
            *((uint16_t*)&linestart[k*fb_bytes]) = color;
        }
    }
}

void fb_power(bool enable) {
    int fd;
    fd = open(fb_powercfg_path, O_RDWR);
    if(fd < 0) {
        printf("FB powerctrl %s open failed: %d\n", fb_powercfg_path, fd);
        return;
    }
    if(enable)
        write(fd, "on\n\0", 3);
    else
        write(fd, "auto\n\0", 5);
    fsync(fd);
    close(fd);
}

int setup_fb(char* dev) {
    char path[256] = {0};
    int fd;
    struct fb_var_screeninfo vinfo;

    strcpy(path, "/dev/");
    strcat(path, dev);
    fb_fd = open(path, O_RDWR);
    if (fb_fd < 0) {
        printf("FB %s open failed: %d\n", path, fb_fd);
        return fb_fd;
    }

    ioctl (fb_fd, FBIOGET_VSCREENINFO, &vinfo);

    fb_width = vinfo.xres;
    fb_height = vinfo.yres;
    fb_bpp = vinfo.bits_per_pixel;
    fb_bytes = fb_bpp / 8;
    fb_data_size = fb_width * fb_height * fb_bytes;
    printf("FB width: %d height: %d bpp: %d bytespp: %d size: %d\n", fb_width, fb_height, fb_bpp, fb_bytes, fb_data_size);

    fb_data = mmap (0, fb_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

    printf("FB mapped\n");
    
    path[0] = '\0';
    strcpy(path, "/sys/class/graphics/fbcon/cursor_blink");
    fd = open(path, O_RDWR);
    if(fd < 0) {
        printf("FBcon cursor_blink %s open failed: %d\n", path, fd);
    } else {
        write(fd, "0\n\0", 2);
        fsync(fd);
        close(fd);
    }

    strcpy(fb_powercfg_path, "/sys/class/graphics/");
    strcat(fb_powercfg_path, dev);
    strcat(fb_powercfg_path, "/device/power/control");

    fb_power(1);

    memset(fb_data, 0, fb_data_size);

    return 0;
}

int setup_input(char* dev) {
    char path[256] = {0};
    
    strcpy(path, "/dev/input/");
    strcat(path, dev);

    kbd_fd = open(path, O_RDONLY);
    if(kbd_fd < 0) {
        printf("KBD %s open failed: %d\n", path, kbd_fd);
        return kbd_fd;
    }
    return 0;
}

void stop_programs() {
    int status;
    // printf("Stop programs vpid %d apid %d\n", v_pid, a_pid);
    if(v_pid != -1) {
        // kill(v_pid, SIGTERM);
        // kill(v_pid, SIGTERM);
        // kill(v_pid, SIGTERM);
        // kill(v_pid, SIGTERM);
        // kill(v_pid, SIGKILL);
        killpg(v_pid, SIGTERM);
        killpg(v_pid, SIGTERM);
        killpg(v_pid, SIGTERM);
        killpg(v_pid, SIGTERM);
        usleep(20000);
        killpg(v_pid, SIGKILL);
        if(waitpid(v_pid, &status, 0) < 0) {
            printf("Vpid waitpid failed: %d\n", errno);
        } else {
            // printf("Vpid kill success: %d\n", status);
        }
        v_pid = -1;
    }
    if(a_pid != -1) {
        // kill(a_pid, SIGTERM);
        // kill(a_pid, SIGTERM);
        // kill(a_pid, SIGTERM);
        // kill(a_pid, SIGTERM);
        killpg(a_pid, SIGTERM);
        killpg(a_pid, SIGTERM);
        killpg(a_pid, SIGTERM);
        killpg(a_pid, SIGTERM);
        usleep(20000);
        killpg(a_pid, SIGKILL);
        // kill(a_pid, SIGKILL);
        if(waitpid(a_pid, &status, 0) < 0) {
            printf("Apid waitpid failed: %d\n", errno);
        }
        a_pid = -1;
    }
}

void switch_mode(enum modes next_mode) {
    if(curr_mode == next_mode)
        return;

    switch(next_mode) {
        case MODE_NO_CONNECTION:
            stop_programs();
            curr_mode = MODE_NO_CONNECTION;
            if(sock_fd != -1) {
                close(sock_fd);
                sock_fd = -1;
            }
            fb_fill(fb_width, fb_height, 0, 0, BGCOLOR);
            fb_power(1);
            break;
        case MODE_NO_INTERNET:
            stop_programs();
            curr_mode = MODE_NO_INTERNET;
            fb_fill(fb_width, fb_height, 0, 0, BGCOLOR);
            fb_power(1);
            break;
        case MODE_IDLE_REQUEST:
            curr_mode = MODE_IDLE_REQUEST;
            fb_fill(fb_width, fb_height, 0, 0, BGCOLOR);
            fb_power(1);
            break;
        case MODE_IDLE:
            stop_programs();
            curr_mode = MODE_IDLE;
            fb_fill(fb_width, fb_height, 0, 0, BGCOLOR);
            fb_display_bmp(logo_bmp_data, logo_bmp_width, logo_bmp_height, fb_width/2 - logo_bmp_width/2, fb_height/2 - logo_bmp_height/2);
            fb_power(1);
            break;
        case MODE_IDLE_SCROFF:
            curr_mode = MODE_IDLE_SCROFF;
            fb_power(0);
            break;
        case MODE_MANUAL_ACTIVE:
            curr_mode = MODE_MANUAL_ACTIVE;
            fb_power(1);
            break;
        case MODE_REQUEST_ACTIVE:
            curr_mode = MODE_REQUEST_ACTIVE;
            fb_fill(fb_width, fb_height, 0, 0, BGCOLOR);
            fb_power(1);

            break;
        case MODE_REQUEST_RESPONDING:
            curr_mode = MODE_REQUEST_RESPONDING;
            fb_power(1);
            break;
        default:
            break;
    };
}

int exec_prog(const char **argv) {
    pid_t   my_pid;
    // printf("\n\nExecuting program\n");
    my_pid = fork();
    if(my_pid < 0) {
        printf("Failed to fork: %d\n", errno);
        return -1;
    } else if(my_pid > 0) {
        printf("Main, started child with pid: %d\n", my_pid);
        return my_pid;
    } else {
        // printf("\n\nChild process\n");
        if (setsid() < 0) {
            printf("SETSID FAIL: %d\n", errno);
            _exit(EXIT_FAILURE);
        }
        // while(1) {}
        execve(argv[0], (char **)argv , NULL);
        _exit(EXIT_FAILURE);   // exec never returns
    }
}

int main(int argc, char** argv) {
    int ret;
    struct pollfd pfd, sock_pfd;
    struct input_event ev;
    // clock_t t_scroff_start, t_reconn_start;
    struct timeval t_scroff_start, t_reconn_start, t_bounce, t_keepalive, t_curr;
    int keepalive_ctr = 0;
    struct sockaddr_in servaddr;
    bool connect_success = false;
    unsigned int len;
    int bounce_speed_x, bounce_speed_y;
    int bounce_pos_x, bounce_pos_y;
    char buff[256] = {0};

    bounce_pos_x = 0;
    bounce_pos_y = 3;
    bounce_speed_x = 5;
    bounce_speed_y = 7;

    printf("Starting mainvicom...\n");
    if(argc != 2) {
        printf("Usage: mainvicom [serveraddr]\n");
        return 1;
    }

    printf("Setting up framebuffer...\n");
    ret = setup_fb("fb0");
    if(ret)
        return ret;

    printf("Loading BMPs...\n");
    read_bmp("./logo.bmp", &logo_bmp_data, &logo_bmp_width, &logo_bmp_height);
    read_bmp("./nocon.bmp", &nocon_bmp_data, &nocon_bmp_width, &nocon_bmp_height);
    read_bmp("./nonet.bmp", &nonet_bmp_data, &nonet_bmp_width, &nonet_bmp_height);

    printf("Setting up keyboard...\n");
    ret = setup_input("event0");
    if(ret)
        return ret;

    printf("Setting up connection with server...\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    servaddr.sin_port = htons(8100);

    printf("Starting main loop\n");

    gettimeofday(&t_scroff_start, NULL);
    t_keepalive = t_bounce = t_scroff_start;
    t_reconn_start.tv_sec = -5;

    pfd.fd = kbd_fd;
    pfd.events = POLLIN;

    switch_mode(MODE_NO_CONNECTION);

    while(!ret) {
        //processing keyboard events
        ret = poll(&pfd, 1, 4);
        if(ret != 0) {
            if(ret < 0) {
                printf("Poll error: %d\n", errno);
                break;
            }
            ret = read(kbd_fd, &ev, sizeof(ev));
            if (ret < (int) sizeof(struct input_event)) {
                printf("expected %d bytes, got %d\n", (int) sizeof(struct input_event), ret);
                break;
            }
            if(ev.type == EV_KEY && ev.value) {
                // printf("KEY PRESS 0x%x\n", ev.code);
                if(curr_mode == MODE_IDLE_SCROFF) {
                    switch_mode(MODE_IDLE);
                }
                gettimeofday(&t_scroff_start, NULL);
                if(ev.code == KEY_ENTER) {
                    if(curr_mode == MODE_IDLE) {
                        //send stream opening request, which should end in switch_mode(MODE_MANUAL_ACTIVE);
                        strcpy(buff, "r\n\0");
                        ret = send(sock_fd, buff, 3, 0);
                        if(ret < 0 && errno != EINPROGRESS) {
                            printf("Connection failed: %d\n", errno);
                            switch_mode(MODE_NO_CONNECTION);
                        }
                        fb_fill(fb_width, fb_height, 0, 0, BGCOLOR);
                    } else if(curr_mode == MODE_REQUEST_ACTIVE) {
                        //accept call request
                        strcpy(buff, "a\n\0");
                        ret = send(sock_fd, buff, 3, 0);
                        if(ret < 0 && errno != EINPROGRESS) {
                            printf("Connection failed: %d\n", errno);
                            switch_mode(MODE_NO_CONNECTION);
                        }
                    } else if(curr_mode == MODE_REQUEST_RESPONDING || curr_mode == MODE_MANUAL_ACTIVE) {
                        //send door open request
                        strcpy(buff, "o\n\0");
                        ret = send(sock_fd, buff, 3, 0);
                        if(ret < 0 && errno != EINPROGRESS) {
                            printf("Connection failed: %d\n", errno);
                            switch_mode(MODE_NO_CONNECTION);
                        }
                    }
                } else if(ev.code == KEY_MENU) {
                    if(curr_mode == MODE_MANUAL_ACTIVE || curr_mode == MODE_REQUEST_RESPONDING || curr_mode == MODE_REQUEST_ACTIVE) {
                        //close cam streaming, if working
                        //send request denial
                        strcpy(buff, "d\n\0");
                        ret = send(sock_fd, buff, 3, 0);
                        if(ret < 0 && errno != EINPROGRESS) {
                            printf("Connection failed: %d\n", errno);
                            switch_mode(MODE_NO_CONNECTION);
                        }
                        switch_mode(MODE_IDLE);
                    }
                }
            }
        }
        
        if(curr_mode != MODE_NO_CONNECTION) {
            sock_pfd.revents = 0;
            sock_pfd.fd = sock_fd;
            sock_pfd.events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
            ret = poll(&sock_pfd, 1, 4);
            if((sock_pfd.revents & POLLERR) || (sock_pfd.revents & POLLHUP) || (sock_pfd.revents & POLLNVAL)) {
                len = sizeof(ret);
                if(getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &ret, &len)) {
                    printf("getsockopt failed: %d\n", errno);
                }
                printf("Connection closed: %d\n", ret);
                switch_mode(MODE_NO_CONNECTION);
            } else {
                ioctl(sock_fd, FIONREAD, &len);
                if(len > 1) {
                    // printf("Data from socket %d bytes!\n", len);
                    ret = recv(sock_fd, buff, 255, 0);
                    if(ret < 0) {
                        printf("Connection failed: %d\n", errno);
                        switch_mode(MODE_NO_CONNECTION);
                    } else if(ret == 0) {
                        printf("No data?\n");
                    } else if(ret > 1) {
                        buff[len] = '\0';
                        switch(buff[0]) {
                            case 's':
                                switch(buff[1]) {
                                    case '0':
                                        //no internet
                                        switch_mode(MODE_NO_INTERNET);
                                        break;
                                    case '1':
                                        //yes internet, no requests
                                        gettimeofday(&t_scroff_start, NULL);
                                        switch_mode(MODE_IDLE);
                                        break;
                                    case '2':
                                        //call request
                                        //send stream request, which should result in switch_mode(MODE_REQUEST_ACTIVE);
                                        if(curr_mode == MODE_MANUAL_ACTIVE) {
                                            switch_mode(MODE_REQUEST_ACTIVE);
                                            strcpy(buff, "a\n\0"); //immediate accept
                                            ret = send(sock_fd, buff, 3, 0);
                                            if(ret < 0 && errno != EINPROGRESS) {
                                                printf("Connection failed: %d\n", errno);
                                                switch_mode(MODE_NO_CONNECTION);
                                            }
                                        } else {
                                            strcpy(buff, "r\n\0");
                                            ret = send(sock_fd, buff, 3, 0);
                                            if(ret < 0 && errno != EINPROGRESS) {
                                                printf("Connection failed: %d\n", errno);
                                                switch_mode(MODE_NO_CONNECTION);
                                            } else {
                                                switch_mode(MODE_IDLE_REQUEST);
                                            }
                                        }
                                        break;
                                    default:
                                        printf("Unknown status: %c\n", buff[1]);
                                        break;
                                }
                                break;
                            case 'v':
                                //video&audio stream can be started
                                const char *startv_argv[3] = {"/usr/bin/bash", "./startv.sh", NULL};
                                if(curr_mode == MODE_IDLE_REQUEST) {
                                    //was call request
                                    if(v_pid == -1) {
                                        v_pid = exec_prog(startv_argv);
                                    }
                                    if(v_pid != -1) {
                                        switch_mode(MODE_REQUEST_ACTIVE);
                                    } else {
                                        printf("startv.sh failed: %d!\n", errno);
                                    }
                                } else {
                                    //was manual request
                                    if(v_pid == -1) {
                                        v_pid = exec_prog(startv_argv);
                                    }
                                    if(v_pid != -1) {
                                        switch_mode(MODE_MANUAL_ACTIVE);
                                    } else {
                                        printf("startv.sh failed: %d!\n", errno);
                                    }
                                }
                                break;
                            case 'a':
                                //reverse audio stream can be started
                                const char *starta_argv[4] = {"/usr/bin/bash", "./starta.sh", argv[1], NULL};
                                if(curr_mode == MODE_REQUEST_ACTIVE) {
                                    //was call request
                                    if(a_pid == -1) {
                                        a_pid = exec_prog(starta_argv);
                                    }
                                    if(a_pid != -1) {
                                        switch_mode(MODE_REQUEST_RESPONDING);
                                    } else {
                                        printf("starta.sh failed: %d!\n", errno);
                                    }
                                }
                                break;
                            case 'e':
                                //stop streams
                                switch_mode(MODE_IDLE);
                                break;
                            case 'p':
                                //ping
                                if(keepalive_ctr < 0) {
                                    keepalive_ctr = 0;
                                }
                                keepalive_ctr++;
                                break;
                            default:
                                printf("Unknown command: %c; %s\n", buff[0], buff);
                                break;
                        }
                    }
                }
            }
        }

        gettimeofday(&t_curr, NULL);
        switch(curr_mode) {
            case MODE_NO_CONNECTION:
                //try connecting to server
                if(timeval_subtract(t_curr, t_reconn_start).tv_sec >= 5) {
                    if(sock_fd != -1) {
                        printf("Connection failed: timeout\n");
                        close(sock_fd);
                        sock_fd = -1;
                    }
                    sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
                    if (sock_fd < 0) {
                        printf("Socket creation failed: %d\n", errno);
                        ret = sock_fd;
                        break;
                    }
                    ret = connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
                    connect_success = true;
                    if(ret != 0 && errno != EINPROGRESS) {
                        printf("Connect failed immediately: %d\n", errno);
                        connect_success = false;
                    } else {
                        printf("Connecting...\n");
                    }
                    gettimeofday(&t_reconn_start, NULL);
                } else if(connect_success) {
                    sock_pfd.revents = 0;
                    sock_pfd.fd = sock_fd;
                    sock_pfd.events = POLLIN | POLLOUT | POLLERR | POLLHUP | POLLNVAL;
                    ret = poll(&sock_pfd, 1, 4);
                    if(ret != 0) {
                        if(ret < 0) {
                            printf("Socket poll error: %d\n", errno);
                            break;
                        }
                        if((sock_pfd.revents & POLLERR) || (sock_pfd.revents & POLLHUP) || (sock_pfd.revents & POLLNVAL)) {
                            len = sizeof(ret);
                            if(getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &ret, &len)) {
                                printf("getsockopt failed: %d\n", errno);
                            }
                            printf("Connection failed: %d\n", ret);
                            connect_success = false;
                            close(sock_fd);
                            sock_fd = -1;
                        } else {
                            printf("Connection success!\n");
                            connect_success = false;
                            switch_mode(MODE_NO_INTERNET);
                            keepalive_ctr = 0;
                        }
                    }
                }

                //bouncing text
                if((timeval_subtract(t_curr, t_bounce).tv_usec/1000) >= 500) {
                    //every 500ms
                    fb_fill(nocon_bmp_width, nocon_bmp_height, bounce_pos_x, bounce_pos_y, BGCOLOR);
                    bounce_pos_x += bounce_speed_x;
                    bounce_pos_y += bounce_speed_y;
                    if(bounce_pos_x < 0) {
                        bounce_speed_x = -bounce_speed_x;
                        bounce_pos_x = -bounce_pos_x;
                    }
                    if(bounce_pos_y < 0) {
                        bounce_speed_y = -bounce_speed_y;
                        bounce_pos_y = -bounce_pos_y;
                    }
                    if(bounce_pos_x > (fb_width - nocon_bmp_width)) {
                        bounce_speed_x = -bounce_speed_x;
                        bounce_pos_x = (fb_width - nocon_bmp_width);
                    }
                    if(bounce_pos_y > (fb_height - nocon_bmp_height)) {
                        bounce_speed_y = -bounce_speed_y;
                        bounce_pos_y = (fb_height - nocon_bmp_height);
                    }
                    fb_display_bmp(nocon_bmp_data, nocon_bmp_width, nocon_bmp_height, bounce_pos_x, bounce_pos_y);
                    gettimeofday(&t_bounce, NULL);
                }
                break;
            case MODE_NO_INTERNET:
                //bouncing text
                if((timeval_subtract(t_curr, t_bounce).tv_usec/1000) >= 500) {
                    //every 500ms
                    fb_fill(nonet_bmp_width, nonet_bmp_height, bounce_pos_x, bounce_pos_y, BGCOLOR);
                    bounce_pos_x += bounce_speed_x;
                    bounce_pos_y += bounce_speed_y;
                    if(bounce_pos_x < 0) {
                        bounce_speed_x = -bounce_speed_x;
                        bounce_pos_x = -bounce_pos_x;
                    }
                    if(bounce_pos_y < 0) {
                        bounce_speed_y = -bounce_speed_y;
                        bounce_pos_y = -bounce_pos_y;
                    }
                    if(bounce_pos_x > (fb_width - nonet_bmp_width)) {
                        bounce_speed_x = -bounce_speed_x;
                        bounce_pos_x = (fb_width - nonet_bmp_width);
                    }
                    if(bounce_pos_y > (fb_height - nonet_bmp_height)) {
                        bounce_speed_y = -bounce_speed_y;
                        bounce_pos_y = (fb_height - nonet_bmp_height);
                    }
                    fb_display_bmp(nonet_bmp_data, nonet_bmp_width, nonet_bmp_height, bounce_pos_x, bounce_pos_y);
                    gettimeofday(&t_bounce, NULL);
                }
                
                if(timeval_subtract(t_curr, t_keepalive).tv_sec >= 5) {
                    strcpy(buff, "p\n\0");
                    ret = send(sock_fd, buff, 2, 0);
                    if(ret < 0 && errno != EINPROGRESS) {
                        printf("Connection failed: %d\n", errno);
                        switch_mode(MODE_NO_CONNECTION);
                    }
                    gettimeofday(&t_keepalive, NULL);
                }
                break;
            case MODE_IDLE:
                //shutting down screen
                if(timeval_subtract(t_curr, t_scroff_start).tv_sec >= 5) {
                    switch_mode(MODE_IDLE_SCROFF);
                }
            default:
                if(timeval_subtract(t_curr, t_keepalive).tv_sec >= 5) {
                    strcpy(buff, "p\n\0");
                    ret = send(sock_fd, buff, 2, 0);
                    if(ret < 0 && errno != EINPROGRESS) {
                        printf("Connection failed: %d\n", errno);
                        switch_mode(MODE_NO_CONNECTION);
                    }
                    if(keepalive_ctr <= -5) {
                        printf("Connection timed out\n");
                        switch_mode(MODE_NO_CONNECTION);
                    }
                    keepalive_ctr--;
                    gettimeofday(&t_keepalive, NULL);
                }
                break;
        };

        ret = 0;
    }

    return ret;
}
