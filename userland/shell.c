#include "syscall.h"
#include "../userprog/syscall.h" // Include fix IntelliSense
#include "lib.h"


#define MAX_LINE_SIZE  60
#define MAX_ARG_COUNT  32
#define ARG_SEPARATOR  ' '

static inline void
WritePrompt(OpenFileId output)
{
    static const char PROMPT[] = "--> ";
    Write(PROMPT, sizeof PROMPT - 1, output);
}

static inline void
WriteError(const char *description, OpenFileId output)
{
    // TODO: how to make sure that `description` is not `NULL`?

    static const char PREFIX[] = "Error: ";
    static const char SUFFIX[] = "\n";

    Write(PREFIX, sizeof PREFIX - 1, output);
    Write(description, strlen(description), output);
    Write(SUFFIX, sizeof SUFFIX - 1, output);
}

static unsigned
ReadLine(char *buffer, unsigned size, OpenFileId input)
{
    // TODO: how to make sure that `buffer` is not `NULL`?

    unsigned i;

    for (i = 0; i < size; i++) {
        Read(&buffer[i], 1, input);
        // TODO: what happens when the input ends?
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
            break;
        }
    }
    return i;
}

static int
PrepareArguments(char *line, char **argv, unsigned argvSize)
{
    // TODO: how to make sure that `line` and `argv` are not `NULL`?, and
    //       for `argvSize`, what precondition should be fulfilled?
    //
    // TODO: use `bool` instead of `int` as return type; for doing this,
    //       given that we are in C and not C++, it is convenient to include
    //       `stdbool.h`.

    unsigned argCount;

    argv[0] = line;
    argCount = 0;

    // Traverse the whole line and replace spaces between arguments by null
    // characters, so as to be able to treat each argument as a standalone
    // string.
    //
    // TODO: what happens if there are two consecutive spaces?, and what
    //       about spaces at the beginning of the line?, and at the end?
    //
    // TODO: what if the user wants to include a space as part of an
    //       argument?
    for (unsigned i = 0; line[i] != '\0'; i++) {
        if (line[i] == ARG_SEPARATOR) {
            if (argCount == argvSize - 1) {
                // The maximum of allowed arguments is exceeded, and
                // therefore the size of `argv` is too.  Note that 1 is
                // decreased in order to leave space for the NULL at the end.
                return 0;
            }
            line[i] = '\0';
            argv[argCount] = &line[i + 1];
            argCount++;
        }
    }
    argv[argCount] = NULL;
    return 1;
}

int
main(void)
{
    const OpenFileId INPUT  = CONSOLE_INPUT;
    const OpenFileId OUTPUT = CONSOLE_OUTPUT;
    char             line[MAX_LINE_SIZE];
    char            *argv[MAX_ARG_COUNT];

    for (;;) {
        WritePrompt(OUTPUT);
        const unsigned lineSize = ReadLine(line, MAX_LINE_SIZE, INPUT);
        if (lineSize == 0) {
            continue;
        }

        int joinable = 1;
        char *ptr = line;
        if (line[0] == '&')
        {
            ptr = line + 1;
            joinable = 0;
        }

        if (PrepareArguments(ptr, argv, MAX_ARG_COUNT) == 0) {
            WriteError("too many arguments.", OUTPUT);
            continue;
        }

        // Comment and uncomment according to whether command line arguments
        // are given in the system call or not.
        
        SpaceId newProc;
        if(strcmp(ptr, ":q"))
        {
            Halt();
        }

        if (argv[0] == NULL)
            newProc = Exec(ptr, joinable, NULL);
        else
            newProc = Exec(ptr, joinable, argv);

        // TODO: check for errors when calling `Exec`; this depends on how
        //       errors are reported.

        if(joinable)
            Join(newProc);

        // TODO: is it necessary to check for errors after `Join` too, or
        //       can you be sure that, with the implementation of the system
        //       call handler you made, it will never give an error?; what
        //       happens if tomorrow the implementation changes and new
        //       error conditions appear?
    }

    // Never reached.
    return -1;
}
