
### Testing the `asgn2` Character Device Driver

1.  **Compile the Driver and Utils:**
    ```bash
    $ make clean
    $ make
    ```
    This creates the binaries `asgn2.ko` and `data_generator`.

2.  **Load the Driver:**
    ```bash
    $ sudo insmod asgn2.ko
    ```

3.  **Verify Driver:**
    ```bash
    $ dmesg | tail
    $ ls -l /dev/asgn2
    ```

4.  **Test Files:**
    ```bash
    $ echo "Hello from session one." > file1.txt
    $ echo "This is the second session." > file2.txt
    ```

5. **Test with Large file (Tasklet):**

    ```
    $ yes "This is a line of text for a large file." | head -n 1000000 > large_file.txt
    ```

6.  **Run the Data Generator:**
    ```bash
    $ sudo ./data_generator file1.txt file2.txt &
    $ sudo ./data_generator large_file.txt &
    ```

7.  **Read the Sessions:**
    ```bash
    $ sudo cat /dev/asgn2
    ```

8.  **Unload the Driver:**
    ```bash
    $ sudo rmmod asgn2
    ```