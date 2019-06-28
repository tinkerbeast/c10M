// freestanding
#include <stddef.h>
// systems
#include <errno.h>
#include <unistd.h>
// libraries
#include <stdio.h>
// local
#include "server.h"

#define SERVER_TRACE 0
#define SERVER_BLOCK 0

static char* default_request_response = 
"HTTP/1.0 200 OK\n"
"Content-type: text/html\n"
"\n"
"<html>\n"
" <body>\n"
"  <h1>Hello Client %d!</h1>\n"
" </body>\n"
"</html>\n";

// read avaiable bytes
static ssize_t reada(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                    /* # of bytes fetched by last read() */

    while(1) {
        numRead = read(fd, buffer, n);
        if (numRead == 0) {               /* EOF */
            return 0;
        } else if (numRead == -1) {
            if (errno == EINTR)
                continue;               /* Interrupted --> restart read() */
            else
                return -1;              /* Some other error */
        } else {
            break;                      /* All avaiable data read/buffer full */
        }
    }

    return numRead;                     /* Must be 'n' bytes if we get here */
}

#if 0
static ssize_t readn(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                    /* # of bytes fetched by last read() */
    size_t totRead;                     /* Total # of bytes read so far */
    char *buf;

    buf = buffer;                       /* No pointer arithmetic on "void *" */
    for (totRead = 0; totRead < n; ) {
        numRead = read(fd, buf, n - totRead);

        if (numRead == 0)               /* EOF */
            return totRead;             /* May be 0 if this is first read() */
        if (numRead == -1) {
            if (errno == EINTR)
                continue;               /* Interrupted --> restart read() */
            else
                return -1;              /* Some other error */
        }
        totRead += numRead;
        buf += numRead;
    }
    return totRead;                     /* Must be 'n' bytes if we get here */
}
#endif


static ssize_t writen(int fd, const void *buffer, size_t n)
{
    ssize_t numWritten;                 /* # of bytes written by last write() */
    size_t totWritten;                  /* Total # of bytes written so far */
    const char *buf;

    buf = buffer;                       /* No pointer arithmetic on "void *" */
    for (totWritten = 0; totWritten < n; ) {
        numWritten = write(fd, buf, n - totWritten);

        if (numWritten <= 0) {
            if (numWritten == -1 && errno == EINTR)
                continue;               /* Interrupted --> restart write() */
            else
                return -1;              /* Some other error */
        }
        totWritten += numWritten;
        buf += numWritten;
    }
    return (ssize_t)totWritten;                  /* Must be 'n' bytes if we get here */
}

server_state_e server_http_process_request(int connector_fd, struct server_http_request * request)
{
    char req_str[1024];
    ssize_t req_len = 0;

    req_len = reada(connector_fd, req_str, sizeof (req_str) - 1);
    if (req_len >= 1024) {
        return SERVER_ERROR;
    } else if (req_len == 0) {
        return SERVER_CLIENT_CLOSED;
    } else if (req_len == -1) {
        return SERVER_CLIENT_ERROR;
    }

    if (SERVER_TRACE) {
        req_str[1024 -1] = '\0';
        printf("%s", req_str);
    }

    // TODO: Actually process the request
    request->dummy = 0;

    return SERVER_CLIENT_CLOSE_REQ; // TODO: Fix the return code based on the request
}


server_state_e server_http_process_response(int connector_fd, const struct server_http_request * request)
{
    static int rsp_count = 0;

    char resp_str[1024];
    ssize_t write_len = 0;
    int resp_len = -1;

    if (request->dummy != 0) {
        // TODO: Actually do something
        return SERVER_ERROR;
    }

    rsp_count++;

    if (SERVER_BLOCK) {
        // TODO: blocking call
    }

    resp_len = snprintf(resp_str, sizeof(resp_str), default_request_response, rsp_count);
    write_len = writen(connector_fd, resp_str, resp_len);
    if (write_len == -1) {
        return SERVER_ERROR;
    }

    return SERVER_OK; // TODO: Return proper code 
}

#if 0
static void http_handle_request (int connection_fd)
{
    char buffer[256];
    ssize_t bytes_read;

    /* Read some data from the client.  */
    bytes_read = read (connection_fd, buffer, sizeof (buffer) - 1);
    if (bytes_read > 0) {
        char method[sizeof (buffer)];
        char url[sizeof (buffer)];
        char protocol[sizeof (buffer)];

        /* Some data was read successfully.  NUL-terminate the buffer so
           we can use string operations on it.  */
        buffer[bytes_read] = '\0';
        /* The first line the client sends is the HTTP request, which is
           composed of a method, the requested page, and the protocol
           version.  */
        sscanf (buffer, "%s %s %s", method, url, protocol);
        /* The client may send various header information following the
           request.  For this HTTP implementation, we don't care about it.
           However, we need to read any data the client tries to send.  Keep
           on reading data until we get to the end of the header, which is
           delimited by a blank line.  HTTP specifies CR/LF as the line
           delimiter.  */
        while (strstr (buffer, "\r\n\r\n") == NULL)
            bytes_read = read (connection_fd, buffer, sizeof (buffer));
        /* Make sure the last read didn't fail.  If it did, there's a
           problem with the connection, so give up.  */
        if (bytes_read == -1) {
            close (connection_fd);
            return;
        }
        /* Check the protocol field.  We understand HTTP versions 1.0 and
           1.1.  */
        if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
            /* We don't understand this protocol.  Report a bad response.  */
            write (connection_fd, bad_request_response, 
                    sizeof (bad_request_response));
        }
        else if (strcmp (method, "GET")) {
            /* This server only implements the GET method.  The client
               specified some other method, so report the failure.  */
            char response[1024];

            snprintf (response, sizeof (response),
                    bad_method_response_template, method);
            write (connection_fd, response, strlen (response));
        }
        else 
            /* A valid request.  Process it.  */
            handle_get (connection_fd, url);
    }
    else if (bytes_read == 0)
        /* The client closed the connection before sending any data.
           Nothing to do.  */
        ;
    else 
        /* The call to read failed.  */
        system_error ("read");

#endif

