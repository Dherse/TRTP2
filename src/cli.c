#include "../headers/cli.h"


/*
 * Refer to headers/cli.h
 */
int parse_receiver(int argc, char *argv[], config_rcv_t *config) {
    int c;

    /** Maximum number of active connections */
    char *m = DEFAULT_MAX_CAPACITY_STR;

    /** Maximum number of packets received in a single syscall */
    char *W = MAX_WINDOW_SIZE_STR;

    /** Maximum advertised window size */
    char *w = MAX_WINDOW_SIZE_STR;

    /** Number of handler thread */
    char *n = DEFAULT_HANDLER_NUM_STR;

    /** Number of receiver thread */
    char *N = DEFAULT_RECEIVER_NUM_STR;

    /** Output file format */
    char *o = DEFAULT_OUT_FORMAT;

    /** Input IP mask */
    char *ip = NULL;

    /** Input port */
    char *port = NULL;

    config->sequential = false;
    optind = 0;
    while((c = getopt(argc, argv, ":m:o:n:w:sN:W:")) != -1) {
        switch(c) {
            case 'm':
                m = optarg;
                break;

            case 'W':
                W = optarg;
                break;

            case 'o':
                o = optarg;
                break;

            case 'w':
                w = optarg;
                break;

            case 'n':
                n = optarg;
                break;

            case 'N':
                N = optarg;
                break;

            case 's':
                config->sequential = true;
                break;

            case ':':
                errno = CLI_O_VALUE_MISSING;
                return -1;

            case '?':
            default:
                errno = CLI_UNKNOWN_OPTION;
                return -1;
        }
    }

    /* IP and port parameters */

    if (optind < argc) {
        ip = argv[optind++];
    } else {
        errno = CLI_IP_MISSING;
        return -1;
    }

    if (optind < argc) {
        port = argv[optind++];
    } else {
        errno = CLI_PORT_MISSING;
        return -1;
    }

    if (optind < argc) {
        errno = CLI_TOO_MANY_ARGUMENTS;
        return -1;
    }

    /* Format quotation marks */

    if (*o == '"') {
        o++;
    }

    int len = strlen(o);
    char *last = o + len - 1;
    if (*last == '"') {
        char *old = o;
        o = malloc(sizeof(char) * len);
        if (o == NULL) {
            errno = FAILED_TO_ALLOCATE;
            return -1;
        }

        memcpy(o, old, len - 1);
        o[len-1] = '\0';
    }

    /* format regex - to ensure one and only one %d in format */

    regex_t regex;
    
    if (regcomp(&regex, REGEX_FORMAT, 0) != 0) {
        errno = REGEX_COMPILATION_FAILED;
        return -1;
    }

    int exec = regexec(&regex, o, 0, NULL, 0) != 0;
    if (!exec) {
        // validation was a success
    } else {
        regfree(&regex);

        errno = CLI_FORMAT_VALIDATION_FAILED;
        return -1;
    }

    regfree(&regex);
    config->format = o;
    config->format_len = strlen(o);

    /* concurrent connection validations */

    int max;
    if (str2int(&max, m, 10) == -1 || max < 0) {
        errno = CLI_MAX_INVALID;
        return -1;
    }

    config->max_connections = (uint16_t) max;

    /* port number validation */

    int port_number;
    if (str2int(&port_number, port, 10) == -1 || port_number < 0 || port_number > 49151) {
        errno = CLI_PORT_INVALID;
        return -1;
    }

    config->port = (uint16_t) port_number;

    /* handle number */

    int handle_num;
    if (str2int(&handle_num, n, 10) == -1 || handle_num < 0) {
        errno = CLI_HANDLE_INVALID;
        return -1;
    }

    config->handle_num = (uint16_t) handle_num;

    /* Receiver count */
    int receive_num;
    if (str2int(&receive_num, N, 10) == -1 || receive_num < 0) {
        errno = CLI_HANDLE_INVALID;
        return -1;
    }

    config->receive_num = (uint16_t) receive_num;

    /* max window size */

    int max_window;
    if (str2int(&max_window, w, 10) == -1 || max_window < 0 || max_window > MAX_WINDOW_SIZE) {
        errno = CLI_WINDOW_INVALID;
        return -1;
    }

    config->max_window = (uint16_t) max_window;

    /* max receive size */

    int receive_size;
    if (str2int(&receive_size, W, 10) == -1 || receive_size < 0) {
        errno = CLI_WINDOW_INVALID;
        return -1;
    }

    config->receive_window_size = (uint16_t) receive_size;

    /* IPv6 validation */

    struct addrinfo hints, *infoptr;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;

    int result = getaddrinfo(ip, port, &hints, &infoptr);
    if (result) {
        errno = CLI_IP_INVALID;

        return -1;
    }

    struct addrinfo *addrIt;
    struct addrinfo *prev;

    for (addrIt = infoptr; addrIt != NULL; prev = addrIt, addrIt = addrIt->ai_next) {
        char ipAddr[INET6_ADDRSTRLEN];
        inet_ntop(addrIt->ai_family, get_socket_addr(addrIt->ai_addr), ipAddr, sizeof ipAddr); 
    }
    
    config->addr_info = prev;

    config->handle_affinities = NULL;
    config->receive_affinities = NULL;

    return 0;
}

/*
 * Refer to headers/cli.h
 */
int parse_affinity_file(config_rcv_t *config) {
    config->handle_affinities = calloc(config->handle_num, sizeof(afs_t));
    if (config->handle_affinities == NULL) {
        return -1;
    }

    config->receive_affinities = calloc(config->receive_num, sizeof(afs_t));
    if (config->receive_affinities == NULL) {
        free(config->handle_affinities);
        config->handle_affinities = NULL;
        return -1;
    }

    FILE *file;
    if ((file = fopen("./affinity.cfg", "r"))) {
        char *line = alloca(sizeof(char) * 3), *token;
        size_t len = 0, i;
        size_t read;

        read = getline(&line, &len, file);
        if (read == -1 || line == NULL) {
            fclose(file);
            free(line);
            fprintf(stderr, "Failed to read RX affinity\n");
            free(config->handle_affinities);
            free(config->receive_affinities);
            config->handle_affinities = NULL;
            config->receive_affinities = NULL;
            return -1;
        }

        i = 0;
        char *line2 = line;
	    while ((token = strsep(&line, ",")) != NULL) {
            if (i >= config->receive_num) {
                fclose(file);
                free(line);
                free(line2);
                fprintf(stderr, "Too many RX affinities\n");
                free(config->handle_affinities);
                free(config->receive_affinities);
                config->handle_affinities = NULL;
                config->receive_affinities = NULL;
                return -1;
            }

            int cpu;
            if (str2int(&cpu, token, 10)) {
                fclose(file);
                free(line);
                free(line2);
                fprintf(stderr, "Failed to parse RX affinity: %s\n", token);
                free(config->handle_affinities);
                free(config->receive_affinities);
                config->handle_affinities = NULL;
                config->receive_affinities = NULL;
                return -1;
            }

            config->receive_affinities[i].cpu = cpu;
            i++;
        }

        free(line2);

        if (i != config->receive_num) {
            fclose(file);
            free(line);
            fprintf(stderr, "Not enough RX affinities (%d)\n", i);
            free(config->handle_affinities);
            free(config->receive_affinities);
            config->handle_affinities = NULL;
            config->receive_affinities = NULL;
            return -1;
        }
        
        read = getline(&line, &len, file);
        if (read == -1) {
            fclose(file);
            free(line);
            fprintf(stderr, "Failed to read HD affinity\n");
            free(config->handle_affinities);
            free(config->receive_affinities);
            config->handle_affinities = NULL;
            config->receive_affinities = NULL;
            return -1;
        }

        i = 0;
        line2 = line;
	    while ((token = strsep(&line, ",")) != NULL) {
            if (i >= config->handle_num) {
                fclose(file);
                free(line);
                free(line2);
                fprintf(stderr, "Too many HD affinities\n");
                free(config->handle_affinities);
                free(config->receive_affinities);
                config->handle_affinities = NULL;
                config->receive_affinities = NULL;
                return -1;
            }

            int cpu;
            if (str2int(&cpu, token, 10)) {
                fclose(file);
                free(line);
                free(line2);
                fprintf(stderr, "Failed to parse HD affinity: %s\n", token);
                free(config->handle_affinities);
                free(config->receive_affinities);
                config->handle_affinities = NULL;
                config->receive_affinities = NULL;
                return -1;
            }

            config->handle_affinities[i].cpu = cpu;
            i++;
        }

        free(line2);

        if (i != config->handle_num) {
            fclose(file);
            free(line);
            fprintf(stderr, "Not enough HD affinities\n");
            free(config->handle_affinities);
            free(config->receive_affinities);
            config->handle_affinities = NULL;
            config->receive_affinities = NULL;
            return -1;
        }

        free(line);
        fclose(file);
    } else {
        fclose(file);
        free(config->handle_affinities);
        free(config->receive_affinities);
        config->handle_affinities = NULL;
        config->receive_affinities = NULL;
        fprintf(stderr, "No affinity file present\n");
    }
    return 0;
}

/*
 * Refer to headers/cli.h
 */
void print_config(config_rcv_t *config) {
    fprintf(stderr, " - - - - - - - - CONFIG - - - - - - - - \n");
    fprintf(stderr, "Maximum advertised window: %d (default %d)\n", config->max_window, MAX_WINDOW_SIZE);
    fprintf(stderr, "Maximum packets per syscall: %d (default %d)\n", config->receive_window_size, MAX_WINDOW_SIZE);
    fprintf(stderr, "Sequential? %s\n", config->sequential ? "yes" : "no");
    if (!config->sequential) {

        fprintf(stderr, " - Number of receivers: %u (default 1)\n", config->receive_num);
        if (config->receive_num > 0 && config->receive_affinities != NULL) {
            fprintf(stderr, "  - Affinities (CPU): ");

            int i;
            for (i = 0; i < config->receive_num; i++) {
                fprintf(stderr, "%d ", config->receive_affinities[i].cpu);
            }

            fprintf(stderr, "\n");
        }

        fprintf(stderr, " - Number of handlers: %u (default 2)\n", config->handle_num);
        if (config->handle_num > 0 && config->handle_affinities != NULL) {
            fprintf(stderr, "  - Affinities (CPU): ");

            int i;
            for (i = 0; i < config->handle_num; i++) {
                fprintf(stderr, "%d ", config->handle_affinities[i].cpu);
            }

            fprintf(stderr, "\n");
        }

        fprintf(stderr, " - Receiver to handler ratio: %.2f (typical 0.5)\n", (float) config->receive_num / (float) config->handle_num);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Output file format: %s\n", config->format);
    fprintf(stderr, "Maximum number of concurrent transfers: %d\n", config->max_connections);

    char ip[46];
    ip_to_string((struct sockaddr_in6 *) config->addr_info->ai_addr, ip);
    fprintf(stderr, "Input IP mask: %s\n", ip);
    fprintf(stderr, "Input port: %d\n", config->port);
    fprintf(stderr, " - - - - - - - - - - - - - - - - - - - -\n");
}

/*
 * Refer to headers/cli.h
 */
int str2int(int *out, char *s, int base) {
    char *end;
    if (s[0] == '\0' || isspace(s[0])) {
        errno = STR2INT_INCONVERTIBLE;
        return -1;
    }
    errno = 0;
    long l = strtol(s, &end, base);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX)) {
        errno = STR2INT_OVERFLOW;
        return -1;
    }
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN)) {
        errno = STR2INT_UNDERFLOW;
        return -1;
    }

    if (*end != '\0' && end != NULL && *end != '\n') {
        errno = STR2INT_NOT_END;
        return -1;
    }

    *out = l;

    return 0;
}

/*
 * Refer to headers/cli.h
 */
void* get_socket_addr(const struct sockaddr *sa) {
    // Cast socketaddr to sockaddr_in to get address and port values from data

    if (sa->sa_family == PF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}