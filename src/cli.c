#include "../headers/cli.h"
#include "../headers/global.h"

/**
 * ## Use :
 *
 * Parses an integer from a string
 * 
 * ## Arguments
 *
 * - `out` - a pointer to an output int
 * - `s` - the string to parse
 * - `base` - the base (e.g base 10)
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 * 
 * ## Source
 * 
 * Obtained from (modified) :
 * https://github.com/cirosantilli/cpp-cheat/blob/c6087065b6b499b949360830aa2edeb4ec2ab276/c/string_to_int.c
 *
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

    if (*end != '\0' && end != NULL) {
        errno = STR2INT_NOT_END;
        return -1;
    }

    *out = l;

    return 0;
}

/**
 * ## Use :
 *
 * Turns a sockaddr into either an IPv4 addr or IPv6 addr.
 * 
 * ## Arguments
 *
 * - `sa` - a socket_addr
 *
 * ## Return value
 * 
 * a pointer to either an IPv4 addr or IPv6
 * 
 * ## Source
 * 
 * We found it last year (2018-2019) and can't find the source
 * so credits to the original author whoever that may be.
 * Sorry :)
 *
 */
void* get_socket_addr(const struct sockaddr *sa) {
    // Cast socketaddr to sockaddr_in to get address and port values from data

    if (sa->sa_family == PF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int parse_receiver(int argc, char *argv[], config_rcv_t *config) {
    int c;

    char *m = "100";
    char *n = "4";
    char *o = "%d";
    char *ip = NULL;
    char *port = NULL;

    /* format and concurrent connection parameters */

    optind = 0;
    while((c = getopt(argc, argv, ":m:o:n:")) != -1) {
        switch(c) {
            case 'm':
                m = optarg;
                break;

            case 'o':
                o = optarg;
                break;

            case 'n':
                n = optarg;
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
    for (addrIt = infoptr; addrIt != NULL; addrIt = addrIt->ai_next) {
        char ipAddr[INET6_ADDRSTRLEN];
        inet_ntop(addrIt->ai_family, get_socket_addr(addrIt->ai_addr), ipAddr, sizeof ipAddr); 
    }
    
    config->addr_info = addrIt;

    return 0;
}