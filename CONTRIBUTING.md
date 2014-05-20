# Contributing

Citra is a brand new project, so we have a great opportunity to keep things clean and well organized early on. As such, coding style is very important when making commits. They aren't very strict rules since we want to be flexible and we understand that under certain circumstances some of them can be counterproductive. Just try to follow as many of them as possible:

### General Rules
* A lot of code was taken from other projects (e.g. Dolphin, PPSSPP, Gekko, SkyEye). In general, when editing other people's code, follow the style of the module you're in (or better yet, fix the style if it drastically differs from our guide).
* Line width is typically 100 characters, but this isn't strictly enforced. Please do not use 80-characters.
* Don't ever introduce new external dependencies into Core
* Don't use any platform specific code in Core
* Use namespaces often

### Naming Rules
* Functions
 * CamelCase, "_" may also be used for clarity (e.g. ARM_InitCore)
* Variables
 * lower_case_underscored
 * Prefix "g_" if global
 * Prefix "_" if internal
 * Prefix "__" if ultra internal
* Classes
 * CamelCase, "_" may also be used for clarity (e.g. OGL_VideoInterface)
* Files/Folders
 * lower_case_underscored
* Namespaces
 * CamelCase, "_" may also be used for clarity (e.g. ARM_InitCore)

### Indentation/Whitespace Style
Follow the indentation/whitespace style shown below. Do not use tabs, use 4-spaces instead.

```cpp
namespace Example {

// Namespace contents are not indented
 
// Declare globals at the top
int g_foo = 0;
char* g_some_pointer; // Notice the position of the *
 
enum SomeEnum {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
};
 
struct Position {
    int x, y;
};
 
// Use "typename" rather than "class" here, just to be consistent
template 
void FooBar() {
    int some_array[] = {
        5,
        25,
        7,
        42
    };
 
    if (note == the_space_after_the_if) {
        CallAfunction();
    } else {
        // Use a space after the // when commenting
    }
 
    // Comment directly above code when possible
    if (some_condition) single_statement();
 
    // Place a single space after the for loop semicolons
    for (int i = 0; i != 25; ++i) {
        // This is how we write loops
    }
 
    DoStuff(this, function, call, takes, up, multiple,
            lines, like, this);
 
    if (this || condition_takes_up_multiple &&
        lines && like && this || everything ||
        alright || then) {
    }
 
    switch (var) {
    // No indentation for case label
    case 1: {
        int case_var = var + 3;
        DoSomething(case_var);
        break;
    }
    case 3:
        DoSomething(var);
        return;
        // Always break, even after a return
        break;
 
    default:
        // Yes, even break for the last case
        break;
    }
 
    std::vector
        you_can_declare,
        a_few,
        variables,
        like_this;
}
 
}
```
