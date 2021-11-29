/**
* FSID example
*/

#include <stdio.h>
#include "fsid.h"

static int register_string_value(fsid_t _fsid, const char* _stringValue)
{
    int integerValue = fsid_insert_string(_fsid, _stringValue);
    printf("Register string value \"%s\" and return integer value: %d\n", _stringValue, integerValue);
    return integerValue;
}

static void check_string_value(fsid_t _fsid, const char* _stringValue, int _value)
{
    int integerValue = fsid_check_string(_fsid, _stringValue);
    printf("Check string value \"%s\" and return integer value: %d - %s\n", _stringValue, integerValue, integerValue >= 0 && integerValue == _value ? "pass" : "fail");
}

static void check_integer_value(fsid_t _fsid, const int _integerValue)
{
    const char* stringValue = NULL;
    fsid_check_value(_fsid, _integerValue, &stringValue, NULL);
    printf("Check integer value %d and return string value \"%s\"\n", _integerValue, stringValue ? stringValue : "<invalid string value>");
}

int main(int argc, char* argv[])
{
    fsid_t fsid;
    int f0 = -1;
    int f1 = -1;
    int f2 = -1;
    int f3 = -1;
    int f4 = -1;
    int f5 = -1;
    int f6 = -1;
    int f7 = -1;
    int f8 = -1;
    int f9 = -1;
    int f10 = -1;
    int f11 = -1;
    int f12 = -1;

    printf("--= Fast string identifier =--\n");

    printf("\nInitialize fsid broker.\n");
    fsid_initialize(&fsid, NULL);

    printf("\nRegister ten string values in the broker.\n");
    f1 = register_string_value(fsid, "one");
    f2 = register_string_value(fsid, "two");
    f3 = register_string_value(fsid, "three");
    f4 = register_string_value(fsid, "four");
    f5 = register_string_value(fsid, "five");
    f6 = register_string_value(fsid, "six");
    f7 = register_string_value(fsid, "seven");
    f8 = register_string_value(fsid, "eight");
    f9 = register_string_value(fsid, "nine");
    f10 = register_string_value(fsid, "ten");
    f0 = register_string_value(fsid, "");

    printf("\nRegister duplicate string values in the broker.\n");
    f2 = register_string_value(fsid, "two");
    f5 = register_string_value(fsid, "five");
    f7 = register_string_value(fsid, "seven");

    printf("\nCheck and return int value for string values.\n");
    check_string_value(fsid, "", f0);
    check_string_value(fsid, "one", f1);
    check_string_value(fsid, "two", f2);
    check_string_value(fsid, "three", f3);
    check_string_value(fsid, "four", f4);
    check_string_value(fsid, "five", f5);
    check_string_value(fsid, "six", f6);
    check_string_value(fsid, "seven", f7);
    check_string_value(fsid, "eight", f8);
    check_string_value(fsid, "nine", f9);
    check_string_value(fsid, "ten", f10);
    check_string_value(fsid, "eleven", f11);
    check_string_value(fsid, "twelve", f12);

    printf("\nCheck and return string value for integer values.\n");
    check_integer_value(fsid, f0);
    check_integer_value(fsid, f1);
    check_integer_value(fsid, f2);
    check_integer_value(fsid, f3);
    check_integer_value(fsid, f4);
    check_integer_value(fsid, f5);
    check_integer_value(fsid, f6);
    check_integer_value(fsid, f7);
    check_integer_value(fsid, f8);
    check_integer_value(fsid, f9);
    check_integer_value(fsid, f10);
    check_integer_value(fsid, f11);
    check_integer_value(fsid, f12);
    check_integer_value(fsid, 100);
    check_integer_value(fsid, -1);

    printf("\nRelease fsid broker.\n");
    fsid_release(fsid);

    return 0;
}
