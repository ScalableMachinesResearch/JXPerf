/*
 * Copyright (C) 2019 Frank Mertens.
 *
 * Distribution and use is allowed under the terms of the zlib license
 * (see kissmalloc/LICENSE).
 *
 */

#include <iostream>
#include <list>

int random_get(const int a, const int b)
{
    const unsigned m = (1u << 31) - 1;
    static unsigned x = 7;
    x = (16807 * x) % m;
    return ((uint64_t)x * (b - a)) / (m - 1) + a;
}

double time_get()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[])
{
    using std::cout;
    using std::endl;
    using std::list;

    const int object_count = 1000000;

    int *object = new int[object_count];

    for (int i = 0; i < object_count; ++i)
        object[i] = random_get(0, object_count);

    typedef list<int> int_list;
    int_list *test_list = new int_list;

    cout <<
        "libc std::list benchmark\n"
        "------------------------\n"
        "\n"
        "n = " << object_count << " (number of objects)\n"
        "\n";

    {
        auto t = time_get();

        for (int i = 0; i < object_count; ++i)
            test_list->push_back(object[i]);

        auto dt = time_get() - t;
        cout << "average latency of std::list<int>::push_back(): " << dt * 1e9 / object_count << " ns" << endl;
    }

    {
        auto t = time_get();

        for (int i = 0; i < object_count; ++i)
            test_list->pop_back();

        auto dt = time_get() - t;
        cout << "average latency of std::list<int>::pop_back() : " << dt * 1e9 / object_count << " ns" << endl;
    }

    cout << endl << endl;

    delete test_list;
    delete[] object;

    return 0;
}
