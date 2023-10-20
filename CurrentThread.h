#ifndef CURRENTTHREAD_H
#define CURRENTTHREAD_H

#pragma once

namespace CurrentThread {
extern __thread int t_cachedTid; // __thread 局部线程存储 与其他线程的同名变量独立 一个线程对他进行操作不会影响其他线程

void cacheTid();

inline int tid()
{
    if (__builtin_expect(t_cachedTid == 0, 0)) {
        cacheTid();
    }
    return t_cachedTid;
}
}

#endif