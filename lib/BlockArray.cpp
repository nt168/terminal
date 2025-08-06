/*
    本文件是 Konsole（一个 X 终端）的一部分。
    版权所有 (C) 2000 Stephan Kulow <coolo@kde.org>

    由 e_k <e_k at users.sourceforge.net> 为 QT4 重写，版权所有 (C)2008

    该程序是自由软件；您可以按照自由软件基金会发布的
    GNU 通用公共许可证第 2 版或（由您选择）任何更高版本的条款
    重新发布和/或修改它。

    发布该程序是希望它会有用，
    但没有任何担保；甚至不包含适销性或
    适用于特定目的的默示保证。详情请参阅
    GNU 通用公共许可证。

    您应该已经随本程序收到 GNU 通用公共许可证的副本；
    如果没有，请写信至自由软件基金会，
    地址：51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA。

*/

#include <QtDebug>

// 自有
#include "BlockArray.h"

// 系统
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>
#include <cstdio>


using namespace Konsole;

static int blocksize = 0;

BlockArray::BlockArray()
        : size(0),
        current(size_t(-1)),
        index(size_t(-1)),
        lastmap(nullptr),
        lastmap_index(size_t(-1)),
        lastblock(nullptr), ion(-1),
        length(0)
{
    // lastmap_index = index = current = size_t(-1);
    if (blocksize == 0) {
        blocksize = ((sizeof(Block) / getpagesize()) + 1) * getpagesize();
    }

}

BlockArray::~BlockArray()
{
    setHistorySize(0);
    Q_ASSERT(!lastblock);
}

size_t BlockArray::append(Block * block)
{
    if (!size) {
        return size_t(-1);
    }

    ++current;
    if (current >= size) {
        current = 0;
    }

    int rc;
    rc = lseek(ion, current * blocksize, SEEK_SET);
    if (rc < 0) {
        perror("HistoryBuffer::add.seek");
        setHistorySize(0);
        return size_t(-1);
    }
    rc = write(ion, block, blocksize);
    if (rc < 0) {
        perror("HistoryBuffer::add.write");
        setHistorySize(0);
        return size_t(-1);
    }

    length++;
    if (length > size) {
        length = size;
    }

    ++index;

    delete block;
    return current;
}

size_t BlockArray::newBlock()
{
    if (!size) {
        return size_t(-1);
    }
    append(lastblock);

    lastblock = new Block();
    return index + 1;
}

Block * BlockArray::lastBlock() const
{
    return lastblock;
}

bool BlockArray::has(size_t i) const
{
    if (i == index + 1) {
        return true;
    }

    if (i > index) {
        return false;
    }
    if (index - i >= length) {
        return false;
    }
    return true;
}

const Block * BlockArray::at(size_t i)
{
    if (i == index + 1) {
        return lastblock;
    }

    if (i == lastmap_index) {
        return lastmap;
    }

    if (i > index) {
        qDebug() << "BlockArray::at() i > index\n";
        return nullptr;
    }

//     if (index - i >= length) {
//         kDebug(1211) << "BlockArray::at() index - i >= length\n";
//         return 0;
//     }

    size_t j = i; // (current - (index - i) + (index/size+1)*size) % size ;

    Q_ASSERT(j < size);
    unmap();

    Block * block = (Block *)mmap(nullptr, blocksize, PROT_READ, MAP_PRIVATE, ion, j * blocksize);

    if (block == (Block *)-1) {
        perror("mmap");
        return nullptr;
    }

    lastmap = block;
    lastmap_index = i;

    return block;
}

void BlockArray::unmap()
{
    if (lastmap) {
        int res = munmap((char *)lastmap, blocksize);
        if (res < 0) {
            perror("munmap");
        }
    }
    lastmap = nullptr;
    lastmap_index = size_t(-1);
}

bool BlockArray::setSize(size_t newsize)
{
    return setHistorySize(newsize * 1024 / blocksize);
}

bool BlockArray::setHistorySize(size_t newsize)
{
//    kDebug(1211) << "setHistorySize " << size << " " << newsize;

    if (size == newsize) {
        return false;
    }

    unmap();

    if (!newsize) {
        delete lastblock;
        lastblock = nullptr;
        if (ion >= 0) {
            close(ion);
        }
        ion = -1;
        current = size_t(-1);
        return true;
    }

    if (!size) {
        FILE * tmp = tmpfile();
        if (!tmp) {
            perror("konsole: cannot open temp file.\n");
        } else {
            ion = dup(fileno(tmp));
            if (ion<0) {
                perror("konsole: cannot dup temp file.\n");
                fclose(tmp);
            }
        }
        if (ion < 0) {
            return false;
        }

        Q_ASSERT(!lastblock);

        lastblock = new Block();
        size = newsize;
        return false;
    }

    if (newsize > size) {
        increaseBuffer();
        size = newsize;
        return false;
    } else {
        decreaseBuffer(newsize);
        int res = ftruncate(ion, length*blocksize);
        Q_UNUSED (res);
        size = newsize;

        return true;
    }
}

void moveBlock(FILE * fion, int cursor, int newpos, char * buffer2)
{
    int res = fseek(fion, cursor * blocksize, SEEK_SET);
    if (res) {
        perror("fseek");
    }
    res = fread(buffer2, blocksize, 1, fion);
    if (res != 1) {
        perror("fread");
    }

    res = fseek(fion, newpos * blocksize, SEEK_SET);
    if (res) {
        perror("fseek");
    }
    res = fwrite(buffer2, blocksize, 1, fion);
    if (res != 1) {
        perror("fwrite");
    }
    //    printf("moving block %d to %d\n", cursor, newpos);
}

void BlockArray::decreaseBuffer(size_t newsize)
{
    if (index < newsize) { // 仍然完全适合
        return;
    }

    int offset = (current - (newsize - 1) + size) % size;

    if (!offset) {
        return;
    }

    // Block 构造函数将来可能会执行某些操作...
    char * buffer1 = new char[blocksize];

    FILE * fion = fdopen(dup(ion), "w+b");
    if (!fion) {
        delete [] buffer1;
        perror("fdopen/dup");
        return;
    }

    int firstblock;
    if (current <= newsize) {
        firstblock = current + 1;
    } else {
        firstblock = 0;
    }

    size_t oldpos;
    for (size_t i = 0, cursor=firstblock; i < newsize; i++) {
        oldpos = (size + cursor + offset) % size;
        moveBlock(fion, oldpos, cursor, buffer1);
        if (oldpos < newsize) {
            cursor = oldpos;
        } else {
            cursor++;
        }
    }

    current = newsize - 1;
    length = newsize;

    delete [] buffer1;

    fclose(fion);

}

void BlockArray::increaseBuffer()
{
    if (index < size) { // 还未绕回一次
        return;
    }

    int offset = (current + size + 1) % size;
    if (!offset) { // 不需要移动
        return;
    }

    // Block 构造函数将来可能会执行某些操作...
    char * buffer1 = new char[blocksize];
    char * buffer2 = new char[blocksize];

    int runs = 1;
    int bpr = size; // 每次运行的块数

    if (size % offset == 0) {
        bpr = size / offset;
        runs = offset;
    }

    FILE * fion = fdopen(dup(ion), "w+b");
    if (!fion) {
        perror("fdopen/dup");
        delete [] buffer1;
        delete [] buffer2;
        return;
    }

    int res;
    for (int i = 0; i < runs; i++) {
        // 在链中释放一个块
        int firstblock = (offset + i) % size;
        res = fseek(fion, firstblock * blocksize, SEEK_SET);
        if (res) {
            perror("fseek");
        }
        res = fread(buffer1, blocksize, 1, fion);
        if (res != 1) {
            perror("fread");
        }
        int newpos = 0;
        for (int j = 1, cursor=firstblock; j < bpr; j++) {
            cursor = (cursor + offset) % size;
            newpos = (cursor - offset + size) % size;
            moveBlock(fion, cursor, newpos, buffer2);
        }
        res = fseek(fion, i * blocksize, SEEK_SET);
        if (res) {
            perror("fseek");
        }
        res = fwrite(buffer1, blocksize, 1, fion);
        if (res != 1) {
            perror("fwrite");
        }
    }
    current = size - 1;
    length = size;

    delete [] buffer1;
    delete [] buffer2;

    fclose(fion);

}

