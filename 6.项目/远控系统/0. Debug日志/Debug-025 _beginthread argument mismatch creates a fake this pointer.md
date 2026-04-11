---
tags: [Remote Control System, debug, cpp, windows, IOCP, thread]
created: 2026-04-09
git: "b0c2a8b + local edits"
---
# Debug-025 _beginthread argument mismatch creates a fake this pointer

> **Phenomenon**: `CEdoyunQueue` crashes in the worker thread with a read access violation near `GetQueuedCompletionStatus(...)`. The debugger shows an abnormal `this` value such as `0x23C`.
> Main note: [[7.8 Bug: _beginthread passes an IOCP handle instead of the queue object]]

---

## 1. Symptom

Observed characteristics:

- the project builds successfully
- the crash happens after the worker thread starts
- the exception is a read access violation
- local variables in `threadMain()` can still be inspected
- member access fails
- the debugger shows `this` as a small value instead of a normal heap address

---

## 2. Narrowing Process

The first visible crash point is near:

```cpp
GetQueuedCompletionStatus(
    m_hCompletionPort,
    &dwTransferred,
    &CompletionKey,
    &pOverlapped,
    INFINITE)
```

At first glance, it is tempting to say that `m_hCompletionPort` itself is the damaged value.

But the more useful debugging observation is:

- local variables are normal
- only member access is unsafe

That shifts suspicion from the member itself to the object pointer:

- `m_hCompletionPort` is read through `this`
- if `this` is fake, every member becomes unreadable

---

## 3. Root Cause

The constructor and the thread entry disagree about the meaning of the thread argument.

Producer side:

```cpp
_beginthread(&CEdoyunQueue<T>::threadEntry, 0, m_hCompletionPort)
```

Consumer side:

```cpp
CEdoyunQueue<T>* thiz = (CEdoyunQueue<T>*)arg;
thiz->threadMain();
```

So the actual bug is:

- the producer passes an IOCP handle value
- the consumer treats that value as a queue object pointer

This creates a fake `this` pointer.

---

## 4. Why It Compiles

This mismatch is not rejected by the compiler because:

- `_beginthread` accepts a `void*` argument
- `threadEntry` also receives a `void*`
- casting `void*` back to `CEdoyunQueue<T>*` is legal syntax

The compiler cannot enforce the semantic meaning of `void*`.

---

## 5. Reusable Debugging Lesson

When a worker thread crashes inside a member function, check these two things immediately:

1. What exact value was passed as the thread argument?
2. Does the thread entry cast it back to the same meaning?

If the debugger shows a very small `this` value, treat that as a strong sign that:

- a handle
- an integer-like token
- or some unrelated pointer-sized value

was accidentally interpreted as an object pointer.

---

## 6. Conclusion

This bug is a classic `void*` bridge error:

- the thread was started with the wrong argument
- the thread entry trusted that argument as an object pointer
- the worker later crashed when trying to access members through the fake `this`

Backlink: [[7.8 Bug: _beginthread passes an IOCP handle instead of the queue object]]
