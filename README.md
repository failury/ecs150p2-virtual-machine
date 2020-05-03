**Reference sources:**

The Text book, piazza, OH recordings.

**Used sources:**

Bubble sort algorithm from Geeks for geeks: https://www.geeksforgeeks.org/bubble-sort/

```c++
int i, j;
int n = Ready.size();
for (i = 0; i < n-1; i++)
    for (j = 0; j < n-i-1; j++)
        if (Ready[j].Priority < Ready[j+1].Priority)
            std::swap(Ready[j], Ready[j+1]);
```

The algorithm is used for sorting the ready list by priority. I was using std::sort with custom compare function but I found that it also switch same priority threads so I switch to custom sort function for ready list.