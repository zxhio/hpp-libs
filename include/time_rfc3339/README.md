# time_rfc3339

Date and time library for RFC3339 with C++11 implementation.

## Usage

Header only, simply include time_rfc3339.h .

### Get date-time relative info

```c++
// get year
int year = time_rfc3339::Time::now().year();

// get day of month
int mday = time_rfc3339::Time::now().day();

// get second offset with monite
int sec = time_rfc3339::Time::now().second();
```

**Note**, second() return value in range [0, 59], leap seconds are not considered here.

### Format time

```c++
std::string t = time_rfc3339::Time::now().format();
```

**1985-04-12T23:20:50.52**
This represents 20 minutes and 50.52 seconds after the 23rd hour of April 12th, 1985 in UTC.

**2021-10-10T05:46:58:343241947+08:00**
This represents 46 minutes and 58.343241947 seconds after the 5th hour of Oct 10th, 2021 with an offset of +08:00 from UTC (Pacific Standard Time). Note that this is equivalent to 2021-10-10T13:46:58:343241947Z in UTC.

~~**1990-12-31T23:59:60Z**~~
This represents the leap second inserted at the end of 1990. **Leap seconds are unpredictable, so this form will not appear in time_rfc3339.**


## Optimization

### Stringify integer

The form of the time-related parameters is fixed. The length of the year is 4, the length of the month and date is 2, not enough length to fill the left side 0.

`strftime()` is a generalized processing scheme, we don't need strftime features other than number to string conversion.

For numeric-to-string optimization, we use a simple and very fast algorithm like **table lookup**.

### Thread local cache time zone

Compared to `localtime_r()`, `gmtime_r()` is 150% faster because it does not calculate time zones. The time zone does not change very often on a server, so we can cache the time zone information and get the time zone information regularly to cope with the situation that the time zone may change during the running of the program.

Introduce `thread_local` to avoid race conditions between threads.

**Note**, for a more faster implementation refer to  musl-libc, which is 10% faster than the glibc `gmtime_r()`.

## TODO

- parse string to Time.
- custom secfrac precision.
- time operations.

## Reference
1. [Date and Time on the Internet: Timestamps](https://www.rfc-editor.org/rfc/inline-errata/rfc3339.html)