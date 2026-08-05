---
title: Linux 1-6 章题目汇总
aliases:
  - Linux 基础与系统编程试题
tags:
  - Linux
  - 系统编程
  - 题解
  - 面试
阅读次数: 0
---

# Linux 1-6 章题目汇总

> 覆盖 `3.Linux` 下第 1 章到第 6 章：常用命令、环境变量、字符与格式化函数、fd 文件 I/O、重定向与同步、文件锁、进程创建与退出、`wait`/`waitpid`、`FILE*` 文件流、权限和目录遍历。
>
> **侧重点**：文件描述符与 `FILE*`、短读短写、两层缓冲、`fork + exec + waitpid`、退出状态、目录权限和遍历约占八成。命令冷门参数和字符分类函数全集不单独考；`vfork`、`on_exit`、`setjmp/longjmp`、进程组细节不反复考；IOCP 属于 Windows 完成模型，本卷不考。
>
> **做法建议**：先独立完成每一节，再展开答案。选择题查概念，找错题查边界，场景题要能讲出失败路径和取舍。
>
> 全卷 52 题：选择 12 题、判断 10 题、代码与行为分析 8 题、找错与修复 8 题、简答 6 题、场景与设计 6 题、低频辨析 2 题。

---

## 一、选择题（高频基础）

**1.** 要找出 `src/` 目录下哪些文件包含字符串 `connect_server`，并显示行号，最合适的是：

- A. `find src -name "connect_server"`
- B. `grep -rn "connect_server" src/`
- C. `ls -R src | grep connect_server`
- D. `cat src/* | find connect_server`

**2.** `chmod 640 config.ini` 表示：

- A. 所有者读写，组只读，其他用户无权限
- B. 所有者读写执行，组只读，其他用户无权限
- C. 所有者读写，组可执行，其他用户无权限
- D. 所有用户都可读写

**3.** Shell 中先执行 `TOKEN=abc`，再启动一个子进程。若希望子进程能读到 `TOKEN`，还需要：

- A. `source TOKEN`
- B. `export TOKEN`
- C. `chmod TOKEN`
- D. `echo TOKEN`

**4.** 解析用户输入的十进制整数，并要求能发现空字符串、尾随垃圾字符和溢出，优先选择：

- A. `atoi`
- B. `sscanf` 且不检查返回值
- C. `strtol`，同时检查 `errno`、`endptr` 和范围
- D. `atof`

**5.** Linux 进程启动后，标准输入、标准输出、标准错误通常对应的 fd 是：

- A. 1、2、3
- B. 0、1、2
- C. 0、2、4
- D. -1、0、1

**6.** 希望“文件存在就报错，不存在才创建”，`open()` 的标志应包含：

- A. `O_CREAT | O_TRUNC`
- B. `O_CREAT | O_APPEND`
- C. `O_CREAT | O_EXCL`
- D. `O_RDONLY | O_EXCL`

**7.** `dup2(fd, STDOUT_FILENO)` 成功后，正确的是：

- A. `stdout` 仍指向终端，只复制了文件内容
- B. fd 1 与 `fd` 指向同一个 open file description，共享文件偏移
- C. `fd` 会被自动关闭
- D. 只能重定向 `printf`，不能重定向 `write(1, ...)`

**8.** 下列关于 `fflush`、`write` 和 `fsync` 的说法，正确的是：

- A. `fflush` 返回就表示数据已经进入磁盘介质
- B. `write` 返回只表示数据通常已交给内核页缓存
- C. `fsync` 只刷新 stdio 的用户态缓冲
- D. `fwrite` 每次都会直接进入内核

**9.** `fork()` 成功后，父子进程对同一个继承 fd 的关系是：

- A. fd 数字相同，但文件偏移完全独立
- B. fd 表项是副本，背后的 open file description 通常共享
- C. 子进程不会继承打开的 fd
- D. 父子进程立即各复制一份文件内容

**10.** `execvp("ls", argv)` 中 `v` 和 `p` 分别表示：

- A. 可变参数、保留 PID
- B. 参数用数组传递、按 `PATH` 搜索程序
- C. 使用虚拟内存、创建新进程
- D. 参数用列表传递、使用父进程环境

**11.** `waitpid(pid, &status, WNOHANG)` 返回 0，表示：

- A. 子进程已经正常退出，退出码为 0
- B. 没有这个子进程
- C. 目标子进程尚未产生可回收的状态，本次没有阻塞
- D. `waitpid` 被信号打断

**12.** `fread(buf, sizeof(Item), 10, fp)` 返回 7，下一步正确的是：

- A. 直接认定发生 I/O 错误
- B. 直接认定正常读到 EOF
- C. 用 `feof(fp)` 和 `ferror(fp)` 区分读完与出错
- D. 再除以 `sizeof(Item)` 才是读取项数

> [!success]- 参考答案（第一节）
> 1. **B**。`grep` 搜内容，`-r` 递归，`-n` 显示行号；`find` 主要按文件名、类型、大小等搜索文件本身。
> 2. **A**。`6 = rw-`，`4 = r--`，`0 = ---`。
> 3. **B**。普通 Shell 变量不进入环境；`export` 后才会被后续子进程继承。
> 4. **C**。`atoi` 无法区分合法的 0 与转换失败，也不能可靠报告溢出。`strtol` 可通过 `endptr` 检查是否完整消费输入，通过 `errno == ERANGE` 和目标类型范围检查溢出。
> 5. **B**。标准输入是 0，标准输出是 1，标准错误是 2。
> 6. **C**。`O_CREAT | O_EXCL` 把“检查不存在”和“创建”合成一个原子操作，文件已存在时失败并设置 `errno = EEXIST`。
> 7. **B**。`dup2` 让新旧 fd 指向同一个 open file description，因此共享文件偏移和打开状态。fd 1 被替换后，`printf` 最终调用的 `write(1, ...)` 和直接写 fd 1 都会进入该文件。
> 8. **B**。`fflush` 把 `FILE*` 库缓冲交给内核；`write` 通常把数据交给页缓存；`fsync` 才要求把该文件的脏数据和必要元数据提交到持久化路径。
> 9. **B**。`fork` 复制 fd 表，但父子表项仍引用同一个 `struct file`，所以一方 `read` 或 `lseek` 会影响另一方看到的偏移。
> 10. **B**。`v` 是 vector，参数放在以空指针结尾的 `argv` 数组中；`p` 表示按 `PATH` 搜索可执行文件。
> 11. **C**。0 只会在带 `WNOHANG` 时出现，表示“现在没状态可取”。-1 才表示出错，需检查 `errno`。
> 12. **C**。`fread` 返回成功读取的项数。短计数可能是 EOF，也可能是错误，必须检查流的两个状态标志。

---

## 二、概念辨析（判断对错并说明理由）

**13.** `read(fd, buf, n)` 返回 0 表示发生错误，应检查 `errno`。

**14.** 对普通阻塞文件调用 `write(fd, buf, n)`，只要返回值不是 -1，就一定写满了 n 字节。

**15.** `dup()` 得到的新 fd 与旧 fd 共享文件偏移。

**16.** `while (!feof(fp))` 是读取文件直到 EOF 的推荐循环写法。

**17.** 写文件时只检查 `fwrite()`，不检查 `fclose()` 也不会漏掉磁盘满等错误。

**18.** `fork()` 后父子进程各自拥有一份 stdio 库缓冲，但继承 fd 背后的 open file description 通常共享。

**19.** `exec*()` 成功后当前进程的 PID 不变，但原来的代码、数据、堆和栈被新程序替换。

**20.** 子进程调用 `exit(42)` 后，父进程 `waitpid()` 写入的 `status` 就等于 42。

**21.** 僵尸进程还保留完整地址空间，所以大量僵尸主要会耗尽内存。

**22.** 删除一个只读文件时，关键通常是父目录是否允许修改目录项，而不是文件自身有没有写权限。

> [!success]- 参考答案（第二节）
> 13. **错**。对普通文件，返回 0 表示已经读到 EOF；返回 -1 才是错误。管道或 socket 的语义还要结合连接状态判断，但错误仍是 -1。
> 14. **错**。`write` 允许短写，管道、socket、非阻塞 fd 和信号打断场景尤其常见。只要返回正数，就推进已写字节数并继续写剩余部分。
> 15. **对**。`dup` 复制 fd 表项，新旧 fd 指向同一个 open file description，文件偏移和 `O_APPEND` 等状态共享。
> 16. **错**。EOF 标志只有一次读取真正撞上末尾后才置位。应让读取函数的返回值控制循环，结束后再用 `feof`/`ferror` 判断原因。
> 17. **错**。最后一批数据可能直到 `fclose` 才从库缓冲写出。此时才遇到磁盘满或写错误，忽略 `fclose` 返回值就会把失败当成功。
> 18. **对**。库缓冲属于进程地址空间，会被复制；fd 背后的 `struct file` 是内核对象，通常由父子共享。
> 19. **对**。`exec` 不创建进程。它替换进程映像，PID、当前工作目录和未设置 `FD_CLOEXEC` 的 fd 等仍可保留。
> 20. **错**。`status` 编码了退出方式和退出值。应先用 `WIFEXITED(status)` 判断正常退出，再用 `WEXITSTATUS(status)` 取出 42。
> 21. **错**。子进程退出时已经释放地址空间和大部分资源。僵尸主要占 PID 和少量内核状态；堆积后会耗尽 PID，导致 `fork` 失败。
> 22. **对**。`unlink` 修改的是目录项，因此主要检查父目录的 `w` 和 `x` 权限。实际系统还要考虑 sticky bit、只读挂载等限制。

---

## 三、代码与行为分析

**23.** 下面程序分别直接运行和重定向到文件时，`hello` 通常各出现几次？为什么？

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("hello\n");
    fork();
    return 0;
}
```

运行方式：

```bash
./a.out
./a.out > out.txt
```

**24.** 下面程序结束后，`out.txt` 中更可能是 `AB` 还是 `BA`？

```c
FILE *fp = fopen("out.txt", "w");
fprintf(fp, "A");
write(fileno(fp), "B", 1);
fclose(fp);
```

**25.** 下面调用有什么问题？

```c
int fd = open("report.txt", O_WRONLY | O_CREAT | O_TRUNC);
```

**26.** 下面循环能否保证把 n 字节全部写完？至少指出两个缺陷。

```c
while (write(fd, buf, n) != n) {
}
```

**27.** 下列代码为什么存在命令注入？用户输入为 `a.txt; touch /tmp/pwned` 时发生什么？

```c
snprintf(cmd, sizeof(cmd), "wc -l %s", filename);
system(cmd);
```

**28.** 子进程调用 `exit(7)`。父进程拿到 `status` 后，下面写法为什么错？

```c
if (status == 7) {
    puts("child failed with 7");
}
```

**29.** 遍历 `/etc/myapp` 时，下面回退代码为什么可能检查错文件？

```c
while ((ent = readdir(dir)) != NULL) {
    if (ent->d_type == DT_UNKNOWN) {
        struct stat st;
        stat(ent->d_name, &st);
    }
}
```

**30.** 缓冲区大小为 8，`snprintf(buf, sizeof(buf), "%s", "123456789")` 的返回值和缓冲区内容有什么特点？如何判断截断？

> [!success]- 参考答案（第三节）
> 23. 直接连终端运行时通常一行；重定向到文件时通常两行。终端上的 stdout 通常行缓冲，换行让 `hello` 在 `fork` 前已经写出；重定向后 stdout 通常全缓冲，`fork` 复制了仍含 `hello` 的缓冲，父子正常退出时各刷一次。稳妥做法是在 `fork` 前 `fflush(NULL)`，或使用不经过 stdio 缓冲的 `write`。
> 24. 更可能是 **`BA`**。`fprintf` 先把 A 放进用户态库缓冲，`write` 直接把 B 交给内核；`fclose` 才把 A 刷出去。混用前先 `fflush(fp)`，或者只使用一套接口。
> 25. 带 `O_CREAT` 时必须提供第三个 `mode_t` 参数，否则是未定义行为，创建权限来自垃圾参数。应写 `open("report.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)`，最终权限还会受 `umask` 影响。
> 26. 不能。第一，短写时下一次仍从 `buf` 开头写 n 字节，会重复数据；第二，返回 -1 时没有处理 `EINTR` 和真实错误；第三，若一直短写或失败，循环可能空转。正确做法是维护 `done`，每轮写 `buf + done` 的剩余部分，正返回就推进，`EINTR` 重试，其他错误退出。
> 27. `system` 会启动 `/bin/sh -c` 解释整条字符串，分号会被当成命令分隔符，因此 `wc` 和 `touch` 都会执行。不要把用户输入拼进 Shell 命令；`fork` 后用 `execvp` 把 `filename` 作为独立的 `argv` 元素传给 `wc`。
> 28. `status` 不是退出码。正确顺序是 `WIFEXITED(status)` 为真后再判断 `WEXITSTATUS(status) == 7`；若子进程被信号杀死，应走 `WIFSIGNALED`/`WTERMSIG` 分支。
> 29. `d_name` 只有条目名，`stat` 会相对当前工作目录解析，不会自动相对正在遍历的目录。可拼出完整路径，但更稳的是 `fstatat(dirfd(dir), ent->d_name, &st, AT_SYMLINK_NOFOLLOW)`。
> 30. 返回值是 9，即“不受缓冲区大小限制时本应写入的字符数”，不含结尾 `\0`。缓冲区只能保存 7 个字符加结尾 `\0`，内容为 `1234567`。返回值小于 0 表示格式化失败，返回值大于等于缓冲区大小表示发生截断。

---

## 四、找错与修复

**31.** 修复下面的文件读取循环，并说明为什么原写法会多处理一次。

```c
int c;
while (!feof(fp)) {
    c = fgetc(fp);
    consume(c);
}
```

**32.** 下面代码想读取 100 个 `Record`。指出对返回值的误解。

```c
size_t n = fread(records, sizeof(Record), 100, fp);
if (n != 100 * sizeof(Record)) {
    perror("fread");
}
```

**33.** 程序只想读取配置，却使用 `fopen("config.ini", "w")`。会发生什么？

**34.** 找出资源所有权问题：

```c
int fd = open("data.bin", O_RDONLY);
FILE *fp = fdopen(fd, "rb");
fclose(fp);
close(fd);
```

**35.** `fork + exec` 中的子进程为什么不该在 `exec` 失败后调用 `exit(127)`？应怎样改？

**36.** 修复下面的非阻塞等待逻辑：

```c
pid_t r;
do {
    r = waitpid(pid, &status, WNOHANG);
} while (r != pid);
```

**37.** 下面循环结束后，怎样区分目录读完和 `readdir` 出错？

```c
while ((ent = readdir(dir)) != NULL) {
    handle(ent);
}
```

**38.** 文件原权限是 `0644`，程序想只给所有者增加执行权限，却调用 `chmod(path, 0100)`。结果是什么？怎样修？

> [!success]- 参考答案（第四节）
> 31. 应写成 `while ((c = fgetc(fp)) != EOF) { consume(c); }`，循环结束后用 `ferror(fp)` 判断是否发生错误。`feof` 只在读取真正撞上末尾后置位，原写法会在最后一个字符之后再进入一次循环，把 `EOF` 当数据处理。
> 32. `fread` 返回成功读取的**项数**，不是字节数。因此应比较 `n != 100`。短读后先用 `ferror(fp)` 判断错误，再用 `feof(fp)` 判断正常到达末尾；`perror` 只在确有错误且 `errno` 有效时使用。
> 33. `"w"` 会创建文件，若文件已经存在则立即截断为 0。只读应使用 `"r"`；需要读写且保留内容用 `"r+"`。模式选择错误可能在第一次读取前就毁掉原文件。
> 34. `fdopen` 成功后，`FILE*` 接管该 fd；`fclose(fp)` 已经关闭底层 fd，再 `close(fd)` 是二次关闭。更危险的是 fd 数字可能已被其他线程复用，第二次 `close` 可能误关别的资源。转换成功后只通过 `fclose` 管理。
> 35. `fork` 复制了父进程的 stdio 缓冲和 `atexit` 注册信息。子进程调用 `exit` 可能重复刷新父进程缓冲、重复执行清理函数。`exec` 失败后报告错误并调用 `_exit(127)`。
> 36. `waitpid` 可能返回 0、目标 PID 或 -1。0 表示尚未结束，可做其他工作后再查；目标 PID 表示已回收；-1 时若 `errno == EINTR` 可重试，其他错误应退出。原循环在 -1 时可能永远等不到 `pid`，形成死循环。
> 37. `readdir` 用同一个 `NULL` 表示结束和错误。调用前把 `errno = 0`；循环结束后若 `errno == 0` 是正常读完，否则用 `perror("readdir")` 报错。若 `handle` 也会修改 `errno`，每轮调用 `readdir` 前都应重置。
> 38. `chmod` 的 mode 是完整新权限，不是增量，结果会变成 `0100`，即只剩所有者执行权限。应先 `stat` 取得 `st_mode`，保留 `st_mode & 07777` 后按位或 `S_IXUSR`，再调用 `chmod`。要注意 `stat` 与 `chmod` 之间存在竞态；已有 fd 时优先 `fchmod`。

---

## 五、简答题（面试口述）

**39.** `FILE*` 和 fd 的关系是什么？各自适合什么场景？

**40.** 从 `fprintf(fp, ...)` 到数据真正落盘，中间经过哪些层？`fflush` 与 `fsync` 各解决哪一段？

**41.** 解释 fd、open file description 和 inode 三层关系。为什么 `dup`、`fork` 和再次 `open` 的行为不同？

**42.** `fork` 与 `exec` 分别做什么？为什么标准模式通常是“子进程 `exec`，父进程 `waitpid`”？`O_CLOEXEC` 解决什么问题？

**43.** `wait` 与 `waitpid` 有什么区别？僵尸进程怎样产生，为什么 `kill -9` 清不掉？

**44.** 目录的 `r`、`w`、`x` 各控制什么？为什么“能按已知名字打开文件”不等于“能列出目录内容”？

> [!success]- 参考答案（第五节）
> 39. fd 是进程 fd 表中的整数下标，内核通过它找到 open file description；`FILE*` 是 libc 在 fd 上包的一层用户态对象，增加库缓冲、格式化、按行读取、EOF 和错误标志。文本配置、简单日志和格式化处理优先 `FILE*`；socket、管道、epoll、文件锁、`mmap`、`dup2`、短写控制和明确持久化时机使用 fd。两者混用时要先同步缓冲，并明确谁负责关闭。
> 40. `fprintf` 通常先把数据复制到 `FILE*` 的用户态库缓冲；缓冲写满、`fflush`、`fclose` 或正常 `exit` 时，libc 调用 `write` 把数据交给内核页缓存；内核稍后回写，或应用调用 `fsync` 要求提交到持久化路径。`fflush` 防止数据困在进程内存，不能保证断电不丢；`fsync` 处理页缓存之后的持久化，但调用前也要先把 stdio 缓冲刷到内核。
> 41. fd 是当前进程可见的编号；open file description（内核 `struct file`）保存文件偏移和打开状态；多个打开实例最后可指向同一个 inode。`dup` 只增加一个指向同一 open file description 的 fd；`fork` 复制 fd 表，但父子表项仍指向原来的 open file description；再次 `open` 通常创建新的 open file description，因此文件偏移独立。三者可能最终指向同一 inode，但共享状态的范围不同。
> 42. `fork` 创建子进程，父子从同一调用点继续；`exec` 不创建进程，而是用新程序替换当前进程映像。常见模式让子进程负责重定向并 `exec`，父进程保留控制逻辑并用 `waitpid` 回收、解析结果。`exec` 默认保留已打开 fd，可能把数据库连接、监听 socket 或敏感文件泄漏给新程序；`O_CLOEXEC`/`FD_CLOEXEC` 让内核在成功 `exec` 时自动关闭这些 fd。
> 43. `wait` 回收任意子进程并阻塞，等价于 `waitpid(-1, &status, 0)`；`waitpid` 可指定 PID 或进程组，还能用 `WNOHANG` 非阻塞查询。子进程退出后，大部分资源已释放，但退出状态要等父进程领取；父进程一直不领取时就形成僵尸。僵尸已经没有用户态代码可执行，`SIGKILL` 无人处理，只能让父进程 `wait`，或结束父进程让它被 PID 1 接管回收。
> 44. 对目录而言，`r` 允许读取目录项名字，`w` 允许新增、删除和重命名目录项，`x` 允许路径穿越和按已知名字访问其中对象。只有 `x` 没有 `r` 时，程序可能成功打开 `dir/known_file`，却不能用 `readdir` 列出有哪些文件；删除通常需要父目录的 `w + x`。

---

## 六、场景与设计题

**45.** 设计一个可靠的 fd 文件复制循环。要求处理 `read` 被信号打断、`write` 短写、读写错误和 fd 生命周期。说明为什么不能假设“一次 `write` 对应一次 `read`”。

**46.** 不使用 `system()`：启动 `grep keyword input.txt`，把子进程标准输出重定向到 `result.txt`，父进程等待并报告正常退出码或终止信号。说出关键调用顺序和每条失败路径。

**47.** 设计一个目录扫描器：列出目标目录下的普通文件，跳过 `.` 和 `..`，正确处理 `readdir` 的错误二义性；当 `d_type == DT_UNKNOWN` 时仍能判断文件类型。说明为什么不能长期保存 `readdir` 返回的裸指针。

**48.** 多个进程向同一个审计日志追加记录，每条记录必须完整，且生成一条记录前还要先读取文件头中的序号、加一、再写回。只使用 `O_APPEND` 是否足够？怎样组合写入和文件锁？

**49.** 一个服务循环 `fork` worker，但从不等待。运行几天后 `fork()` 开始返回 `EAGAIN`，`ps` 中有大量 `<defunct>`。给出确认、修复和容器内的额外注意点。

**50.** Shell 执行 `producer | consumer`，用户按 `Ctrl+C` 时希望两个进程一起结束。为什么不能只记住两个 PID 再逐个发信号？进程组怎样解决？

> [!success]- 参考答案（第六节）
> **45. 可靠复制的核心循环**
>
> 假设 `src`、`dst` 由 RAII fd 包装器持有，析构时关闭：
>
> ```cpp
> std::array<std::byte, 4096> buffer{};
> for (;;) {
>     ssize_t n = ::read(src.get(), buffer.data(), buffer.size());
>     if (n == 0) break;
>     if (n < 0) {
>         if (errno == EINTR) continue;
>         throw std::system_error(errno, std::generic_category(), "read");
>     }
>
>     ssize_t done = 0;
>     while (done < n) {
>         ssize_t m = ::write(dst.get(), buffer.data() + done, n - done);
>         if (m < 0) {
>             if (errno == EINTR) continue;
>             throw std::system_error(errno, std::generic_category(), "write");
>         }
>         if (m == 0) {
>             throw std::runtime_error("write made no progress");
>         }
>         done += m;
>     }
> }
> ```
>
> `read` 返回多少，本轮就只写多少；`write` 的正返回值可能小于剩余量，因此必须推进偏移继续写。RAII 负责所有正常和异常路径上的关闭，避免在多个 `return` 分支里漏关 fd。
>
> ```mermaid
> sequenceDiagram
>     participant U as 复制逻辑
>     participant K as Linux 内核
>     participant S as 源文件
>     participant D as 目标文件
>
>     loop 直到 read 返回 0
>         U->>K: read(src, buffer, capacity)
>         K->>S: 读取下一批字节
>         K-->>U: n 大于 0，或 -1/EINTR
>         alt 被信号打断
>             U->>K: 重试 read
>         else 读到 n 字节
>             loop done 小于 n
>                 U->>K: write(dst, buffer + done, n - done)
>                 K->>D: 写入本轮可接受的 m 字节
>                 K-->>U: m 大于 0，或 -1/EINTR
>             end
>         end
>     end
> ```
>
> 图中两层循环分别处理“读下一批”和“把这一批完整写完”，这是单次 `read`/`write` 调用看不出的错误边界。
>
> **46. 调用顺序与失败路径**
>
> 1. 父进程 `fork()`；失败则父进程直接报告错误。
> 2. 子进程 `open("result.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)`；失败后写入 stderr 并 `_exit(126)`。
> 3. 子进程 `dup2(out_fd, STDOUT_FILENO)`，成功后关闭多余的 `out_fd`。
> 4. 子进程构造 `argv = {"grep", "keyword", "input.txt", NULL}`，调用 `execvp`；只有失败才返回，随后 `_exit(127)`。
> 5. 父进程用 `waitpid(child, &status, 0)`；`EINTR` 重试。成功后先判 `WIFEXITED`，再取 `WEXITSTATUS`；或判 `WIFSIGNALED`，再取 `WTERMSIG`。
>
> ```mermaid
> sequenceDiagram
>     participant P as 父进程
>     participant K as Linux 内核
>     participant C as 子进程
>     participant G as grep
>
>     P->>K: fork()
>     K-->>P: 返回 child PID
>     K-->>C: 返回 0
>     P->>K: waitpid(child, status, 0)
>     activate P
>     Note over P: 阻塞等待子进程
>     C->>K: open(result.txt)
>     C->>K: dup2(out_fd, STDOUT_FILENO)
>     C->>K: execvp(grep, argv)
>     K->>G: 替换子进程映像
>     G->>K: exit(code)
>     K-->>P: waitpid 返回并写入 status
>     deactivate P
> ```
>
> 这套写法把文件名和关键字作为独立参数传给 `grep`，没有 Shell 解析，因此不会把分号、管道符等当成命令语法。
>
> **47. 目录扫描器**
>
> `opendir` 成功后，把 `errno` 清零并循环 `readdir`。先跳过 `.` 和 `..`；`d_type == DT_REG` 可直接认定普通文件；`DT_UNKNOWN` 时调用 `fstatat(dirfd(dir), d_name, &st, AT_SYMLINK_NOFOLLOW)`，再用 `S_ISREG(st.st_mode)` 判断。`readdir` 返回 NULL 后检查 `errno`，最后检查 `closedir` 返回值。
>
> `readdir` 返回的指针指向 libc 管理的内部缓冲，后续读取下一批目录项时可能覆盖原内容。需要保留名字就立刻复制到 `std::string`，不能只保存 `dirent*`。遍历顺序也没有保证，需要有序输出时自行排序。
>
> **48. 审计日志追加**
>
> 单条记录已经准备好时，`O_APPEND` 能把“定位到末尾”和单次 `write` 合成原子动作；仍应把一条记录组装成连续缓冲并用一次 `write` 提交，同时处理短写。这里还有“读序号、加一、写回、追加记录”多个步骤，`O_APPEND` 无法把整个事务锁住。
>
> 所有参与进程需遵守同一套建议锁协议。只需整文件互斥可用 `flock(fd, LOCK_EX)`；需要把头部序号区和日志区分开并行时，用 `fcntl`/OFD 记录锁锁定明确字节范围。持锁期间完成读改写和追加，成功后解锁。锁不能阻止完全不检查锁的程序写文件，因此协议必须覆盖全部写入者。
>
> **49. 僵尸堆积排查**
>
> `ps -o pid,ppid,state,cmd` 确认大量状态为 `Z` 的子进程及共同 PPID；服务的 `fork` 返回 `EAGAIN` 与 PID 耗尽吻合。修复方式取决于主循环：没有别的工作时直接阻塞 `wait`；并发服务用 `SIGCHLD` 配合 `while (waitpid(-1, NULL, WNOHANG) > 0)` 一次收净，并保存、恢复 `errno`。也可明确忽略 `SIGCHLD`，但会失去退出状态。
>
> 容器里的 PID 1 往往是业务进程，不一定具备 init 的回收循环。需要让应用自己回收，或使用 tini、`docker run --init` 等最小 init。`kill -9` 僵尸无效，因为它已经没有代码可执行。
>
> **50. 进程组与终端信号**
>
> 管道中的多个进程属于同一个作业，Shell 把它们放进同一进程组。PID 逐个管理容易漏掉后续子进程，也存在进程退出和 PID 复用的竞态；终端也不会替 Shell 逐个发信号。
>
> Shell 用 `setpgid` 让 `producer` 和 `consumer` 共享 PGID，再用 `tcsetpgrp` 把该组设为前台进程组。用户按 `Ctrl+C` 时，终端驱动把 `SIGINT` 发给整个前台进程组。程序也可用 `killpg(pgid, SIGINT)`，等价于 `kill(-pgid, SIGINT)`。

---

## 七、专项与低频辨析

**51.** 为什么 C++ 代码通常不该用 `setjmp/longjmp` 实现异常？信号处理函数确实需要非局部跳转时，应使用哪一组函数？

**52.** `fseek/ftell` 与 `lseek` 分别属于哪套接口？为什么管道、socket 和终端通常不能用它们求“长度”？在普通文件中越过 EOF 后再写会发生什么？

> [!success]- 参考答案（第七节）
> 51. `longjmp` 直接恢复保存的栈指针和执行位置，中间栈帧的清理代码不会执行；在 C++ 中跨过需要执行的非平凡析构函数属于未定义行为，RAII 资源会失去正常释放路径。因此 C++ 应使用语言异常或显式错误返回。若 C 的信号处理函数确实要跳转，应使用 `sigsetjmp`/`siglongjmp` 保存和恢复信号掩码，并严格遵守异步信号安全限制。
> 52. `fseek/ftell` 操作 `FILE*`，`lseek` 操作 fd。它们依赖底层对象存在可随机访问的文件偏移；管道、socket 和终端是字节流或设备，没有“第几个文件字节”的稳定含义，调用通常失败并设置 `errno = ESPIPE`。普通文件允许把偏移移到 EOF 之后，随后写入会在中间形成空洞；空洞读取为零，但文件系统可不为这些零字节分配实际数据块，这就是稀疏文件。大文件应使用 `fseeko/ftello` 或足够宽的 `off_t`，不能默认 `long` 一定装得下偏移。

---

## 八、自测对照表

| 题号 | 主要考点 | 对应笔记 |
|---|---|---|
| 1-3 | 常用命令、权限数字、环境变量继承 | [[1.0 命令速查表]]、[[1.2 cp、rm、chmod、vim、gcc]]、[[2.2 环境变量]] |
| 4, 30 | 安全数值转换、`snprintf` 截断判断 | [[3.2 数据转换函数]]、[[3.3 格式化输出函数]] |
| 5-9, 13-15, 25-26, 41 | fd、`open/read/write`、`dup2`、open file description | [[4.1 打开、读取、写入、关闭]]、[[4.2 重定向、同步]] |
| 8, 17-18, 23-24, 34, 39-40 | 两层缓冲、`FILE*`/fd、`fflush`/`fsync`、混用与 fork 缓冲 | [[4.2 重定向、同步]]、[[6.2 高级文件操作函数]]、[[6.10 FILE 流与 fd 两套接口总结]] |
| 12, 16-17, 31-33, 37 | `fread/fgets/fgetc` 返回值、EOF 与错误标志 | [[6.1 基本文件函数]]、[[6.3 读取文件]]、[[6.4 写入文件]]、[[6.6 判断错误]] |
| 9-11, 18-21, 27-28, 35-36, 42-43 | `fork/exec/exit/waitpid`、退出状态、僵尸 | [[11.2 进程创建与程序替换]]、[[11.3 结束进程]]、[[11.4 等待与回收]] |
| 22, 29, 37-38, 44, 47 | 目录权限、`chmod`、`opendir/readdir`、`fstatat` | [[3.5 权限控制函数]]、[[6.7 创建删除目录]]、[[6.8 设置权限]]、[[6.9 操作目录]] |
| 45-47 | 健壮复制、重定向执行、目录扫描 | [[4.1 打开、读取、写入、关闭]]、[[4.2 重定向、同步]]、[[11.2 进程创建与程序替换]]、[[11.4 等待与回收]]、[[6.9 操作目录]] |
| 48 | `O_APPEND` 与跨进程文件锁 | [[4.3 文件锁]]、[[6.10 FILE 流与 fd 两套接口总结]] |
| 49 | SIGCHLD、循环回收、容器 PID 1 | [[11.4 等待与回收]] |
| 50 | 进程组、前台作业、组信号 | [[11.6 进程组与会话]] |
| 51 | 非局部跳转与 C++ 析构 | [[11.7 非局部跳转]] |
| 52 | `fseek/ftell`、`lseek`、不可定位流与稀疏文件 | [[4.2 重定向、同步]]、[[6.5 移动文件指针]] |

---

## 九、复习优先级

1. 先重做 8、9、11、12、14、16-18、20、23-26、28-29、31-38、39-49。这些题覆盖系统编程最常见的错误路径。
2. 命令题会写常用组合即可，不必背同一个选项字母在所有命令里的含义。
3. 字符分类函数掌握 `isdigit`、`isspace`、`isalpha` 的使用边界即可；不需要逐个背完整函数表。
4. `vfork`、`on_exit` 和 `setjmp/longjmp` 先做到“能辨析、不误用”。IOCP 属于 Windows 专项，除非岗位描述明确要求，否则不纳入这套 Linux 试题。
