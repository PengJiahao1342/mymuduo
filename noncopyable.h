#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

#pragma once

/*
 * 设置一个noncopyable类，使得noncopyable类被继承后，
 * 派生类对象可以正常构造与析构，但是默认无法进行拷贝构造和赋值操作
 */
class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

#endif