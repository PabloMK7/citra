# Reporting Issues

**The issue tracker is not a support forum.** Unless you can provide precise *technical information* regarding an issue, you *should not post in it*. If you need support, first read the [FAQ](https://github.com/citra-emu/citra/wiki/FAQ) and then either visit our IRC channel, [our forum](https://discuss.citra-emu.org/) or ask in a general emulation forum such as [/r/emulation](https://www.reddit.com/r/emulation/). If you post support questions, generic messages to the developers or vague reports without technical details, they will be closed and locked.

If you believe you have a valid issue report, please post text or a screenshot from the log (the console window that opens alongside Citra) and build version (hex string visible in the titlebar and zip filename), as well as your hardware and software information if applicable.

# Contributing
Citra is a brand new project, so we have a great opportunity to keep things clean and well organized early on. As such, coding style is very important when making commits. We run clang-format on our CI to check the code. Please use it to format your code when contributing. However, it doesn't cover all the rules below. Some of them aren't very strict rules since we want to be flexible and we understand that under certain circumstances some of them can be counterproductive. Just try to follow as many of them as possible:

### General Rules
* A lot of code was taken from other projects (e.g. Dolphin, PPSSPP, Gekko, SkyEye). In general, when editing other people's code, follow the style of the module you're in (or better yet, fix the style if it drastically differs from our guide).
* Line width is typically 100 characters. Please do not use 80-characters.
* Don't ever introduce new external dependencies into Core
* Don't use any platform specific code in Core
* Use namespaces often
* Avoid the use of C-style casts and instead prefer C++-style `static_cast` and `reinterpret_cast`. Try to avoid using `dynamic_cast`. Never use `const_cast`. The only exception to this rule is for casting between two numeric types, where C-style casts are encouraged for brevity and readability.

### Naming Rules
* Functions: `PascalCase`
* Variables: `lower_case_underscored`. Prefix with `g_` if global.
* Classes: `PascalCase`
* Files and Directories: `lower_case_underscored`
* Namespaces: `PascalCase`, `_` may also be used for clarity (e.g. `ARM_InitCore`)

### Indentation/Whitespace Style
Follow the indentation/whitespace style shown below. Do not use tabs, use 4-spaces instead.

### Comments
* For regular comments, use C++ style (`//`) comments, even for multi-line ones.
* For doc-comments (Doxygen comments), use `/// ` if it's a single line, else use the `/**` `*/` style featured in the example. Start the text on the second line, not the first containing `/**`.
* For items that are both defined and declared in two separate files, put the doc-comment only next to the associated declaration. (In a header file, usually.) Otherwise, put it next to the implementation. Never duplicate doc-comments in both places.

```cpp
// Includes should be sorted lexicographically
// STD includes first
#include <map>
#include <memory>

// then, library includes
#include <nihstro/shared_binary.h>

// finally, citra includes
#include "common/math_util.h"
#include "common/vector_math.h"

// each major module is separated
#include "video_core/pica.h"
#include "video_core/video_core.h"

namespace Example {

// Namespace contents are not indented

// Declare globals at the top
int g_foo{}; // {} can be used to initialize types as 0, false, or nullptr
char* g_some_pointer{}; // Pointer * and reference & stick to the type name, and make sure to initialize as nullptr!

/// A colorful enum.
enum SomeEnum {
    ColorRed,   ///< The color of fire.
    ColorGreen, ///< The color of grass.
    ColorBlue,  ///< Not actually the color of water.
};

/**
 * Very important struct that does a lot of stuff.
 * Note that the asterisks are indented by one space to align to the first line.
 */
struct Position {
    int x{}, y{}; // Always intitialize member variables!
};

// Use "typename" rather than "class" here
template <typename T>
void FooBar() {
    const std::string some_string{ "prefer uniform initialization" };

    int some_array[]{
        5,
        25,
        7,
        42,
    };

    if (note == the_space_after_the_if) {
        CallAfunction();
    } else {
        // Use a space after the // when commenting
    }

    // Place a single space after the for loop semicolons, prefer pre-increment
    for (int i{}; i != 25; ++i) {
        // This is how we write loops
    }

    DoStuff(this, function, call, takes, up, multiple,
            lines, like, this);

    if (this || condition_takes_up_multiple &&
        lines && like && this || everything ||
        alright || then) {

        // Leave a blank space before the if block body if the condition was continued across
        // several lines.
    }

    switch (var) {
    // No indentation for case label
    case 1: {
        int case_var{ var + 3 };
        DoSomething(case_var);
        break;
    }
    case 3:
        DoSomething(var);
        return;

    default:
        // Yes, even break for the last case
        break;
    }

    std::vector<T> you_can_declare, a_few, variables, like_this;
}

}
```
