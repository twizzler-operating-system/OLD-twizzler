Debugging Support                        {#debug}
=================


Assertions
----------

Assertions provided are like the C library <tt>assert</tt> function. If the assertion fails, the
kernel will panic. Assertions are replaced by a no-op if <tt>CONFIG_DEBUG</tt> is disabled, thus you
cannot put any code with side-effects in an assert.
