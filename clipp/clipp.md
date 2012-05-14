Title:  CLIPP User Manual
Author: Christopher Alfeld <calfeld@qualys.com>
Data:   May, 2012

CLIPP User Manual
=================

Christopher Alfeld <calfeld@qualys.com><br>
May, 2012

Introduction
------------

CLIPP (pronounced clip-pea) is a framework for generating, manipulating, and
consuming IronBee event streams.

Examples
--------

Send a ModSecurity Audit Log, `february.log`, to a local IronBee configured 
with `ironbee.conf``.

    clipp modsec:february.log ironbee:ironbee.conf
    
`modsec` is a generator. It produces event streams. `ironbee` is a consumer. 
It consumes the event streams, sending them to a local IronBee engine.
    
As previous, but also display the event stream to standard out:

    clipp modsec:february.log @view ironbee:ironbee.conf

`@view@` is a modifier. It takes event streams as inputs, displays them to 
standard out, and passes them on to the next element in the stage.

Multiple generators are possible:

    clipp modsec:february.log modsec:march.log ironbee:ironbee.conf
    
The consumer must be unique though. Generators are processed in order, so all 
events from `february.log` will be sent before events in `march.log`.

Modifiers can be attached to either generators or consumers. Below, the 
`@view` modifier is attached only to the first generator and events from 
`march.log` will not be displayed:

    clipp modsec:february.log @view modsec:march.log ironbee:ironbee.conf

To see all events we could attach an `@view` modifier to the `march.log` 
generator or we could move the modifier to be attached to the consumer 
instead:

    clipp modsec:february.log modsec:march.log ironbee:ironbee.conf @view

Spaces between components and modifiers are optional. Removing them, may make 
which components they are attached to clearer.

    clipp modsec:february.log modsec:march.log ironbee:ironbee.conf@view
    
There are many generators, modifiers, and consumers.  See below for specifics.

Configuration
-------------

Configuration consists of chains.  Each chain is a generator or a consumer
and zero or more modifiers.  The last chain listed in the configuration is
interpreted as the consumer and all others as generators.  There must be at
least two chains: a generator and a consumer.

Generators, consumers, and modifiers are all components.  Components are 
written as `name`:`argument` or, if `argument` is the empty string, just 
`name`. Modifiers are distinguished by beginning with `@`. The component
they modify is referred to as the *base* component. Modifiers may be separated 
from earlier components (base or previous modifier) by white space, but this 
is optional.

Component names only consist of lower case letters and underscores.  Arguments
may contain spaces but, if so, must be in double quotes.  Note that these 
quotes must make it to `clipp`, which usually means they are escaped on the
command line, e.g.,:

    clipp modsec:\"my log.log\" ironbee:ironbee.conf
    
Formally, the configuration grammar is, in pseudo-BNF:

    configuration := generators WS+ consumer
    generators    := generator ( WS+ generator )*
    generator     := chain
    consumer      := chain
    chain         := base modifiers
    modifiers     := ( WS* modifier )*
    modifier      := AT component
    base          := component
    component     := name [ COLON configuration ]
    configuration := quoted | unquoted
    quoted        := /"[^"]+"/
    unquoted      := /[^\s]+/
    name          := /[a-z_]+/
    AT            := "@"
    COLON         := ":"
    WS            := " "
    
The grammar is likely to change in in the near future.  

All arguments after the flags are joined with whitespace and treated as 
configuration text.  You may also ask `clipp` to load configuration from a 
file via `-c` *path*.  Configuration files may have comments by beginning a 
line with `#`.  All lines in the configuration file are otherwise joined with 
whitespace.  E.g.,

    # input.log
    modsec:input.log
    # and then a single input at the end to 127.0.0.1:80
    raw:a,b
       @set_local_ip:127.0.0.1
       @set_local_port:80
    # all fed to ironbee with IDs displayed
    ironbee:ironbee.conf
       @view:id

Input
-----

An *Input* is the fundamental unit of data in `clipp`.  Generators produce 
them, modifiers modify them, and consumers consumer them.  An Input represents 
a single connection with zero or more transactions.  The Input format is 
oriented around IronBee server plugin events and is designed to adapt to 
future changes such as additional events.

An Input is an ID for human consumption and a Connection.  The ID is optional,
and is mainly used by `@view` and `@view:id`, i.e., displayed to standard out
for human consumption.

A Connection is a list of pre-transaction Events, a list of Transactions, and
a list of post-transaction Events.

A Transaction is a list of Events.

An Event is an event identifier (which IronBee Event it corresponds to), a
pre-delay and a post-delay, both of which are floating point values measured
in seconds.  Some consumers, e.g., `ironbee` will interpreted the pre- and 
post- delay values by delaying for that many seconds before and after the 
event is fired.

The following Events are currently defined:

**connection opened** --- Pre transaction only; contains local and remote IP 
and ports.

**connection closed** --- Post transaction only; contains no data.

The remaining Events may only occur in a Transaction.

**connection data in** --- Contains data.

**connection data out** --- Contains data.

**request started** --- Contains some of raw data, method, uri, and protocol.

**request header** --- Contains zero or more headers, each of which is a name
and value.

**request body** -- Contains data.

**request finished** --- Contains no data.

**response started** --- Contains some of raw data, status, message, and 
protocol.

**response header** --- Contains zero or more headers, each of which is a name
and value.

**response body** -- Contains data.

**response finished** --- Contains no data.

Typically, there are two classes of Inputs:

1. Consists of a connection open event, then a sequence of transactions each
consisting of a connection data in and a connection data out event, and then a
connection closed event.  These inputs are typical of generators that read
other formats including `modsec`, `apache`, `raw`, and `suricata`.  
Unmodified, they are usually consumed by an IronBee that uses modhtp to 
parse them.

2. Consists of a connection open event, then a sequence of transactions each
consisting of request started, request header, request body, request finished, 
response started, response header, response body, response finished, and then
a connection closed event.  These are meant to represent input to IronBee from
a source that already does basic parsing.

Other Inputs beyond the above two are certainly possible.  E.g., connection 
data in might occur several times in a row to test data that comes in batches.


Protobuf File Format
--------------------

The CLIPP protobuf file format and it associated components: `pb` and 
`writepb` are especially important.  The protobuf format completely captures 
the Input is the most powerful and flexible format.

The Protobuf format is compact to begin with and uses gzip compression to 
further reduce its size.  If you have a lot of data that you are going to run
through `clipp` multiple times, consider converting it to protobuf format
first via `writepb`.

Generators
----------

**pb**:*path* 

Generate Input from CLIPP Protobuf file.

**raw**:*request*,*response* 

Generate events from a pair of raw files.  Bogus IP and ports are used for the
connection opened event.  You can override those with the `@set_`*X* 
modifiers.

This generator produces a single input with a single transaction.  A 
connection opened and connection closed event are included along with a 
single pair of connection data in and connection data out events in the 
transaction.

**modsec**:*path* --- Generate events from ModSecurity audit log.

ModSecurity audit logs are often somewhat corrupted.  CLIPP will emit a 
message, ignore, and continue processing whenever it fails to parse an entry.

This generator produces an Input for each audit log entry.  The Input consists
of a single transaction with the request and response.

**apache**:*path* --- Generate events from an Apache log.

The log must be in NCSA format:

    "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-agent}i\""
    
Only `Referer` and `User-Agent` headers are included.  No bodies are included.

This generator produces an Input for each line of the log.  The Input consists
of a single transaction with the fabricated request and response.

**suricata**:*path* --- Generate events from a Suricata log.

This generator behaves almost identically to `apache` but reads Suricata log
format instead. 

**htp**:*path: --- Generate events from a libHTP test file.

A libHTP test file is a sequence of request and response blocks.  Request 
blocks begin with `>>>` on a line by itself and response blocks begin with
`<<<` on a line by itself. 

This generator produces a single Input from the file.  The Input consists of
a transaction for every pair of request and response blocks.  The connction
opened event has bogus information.

You may omit response blocks in which case they default to the empty string.  
You may not omit request blocks.

Modifiers
---------

**@view**

All Events are output to standard out in human readable format.  Unprintable
characters are represented by `[`*X*`]` where *X* is the decimal value.

**@view:id**

This modifier is identical to `@view` except only the IDs of each input are 
displayed.

**@view:summary**

This modifier is identical to `@view` except a summary of each input is 
displayed.  The summary is the ID, connection information, and number of
transactions.

**@set_local_ip**:*ip*<br>
**@set_local_port**:*port*<br>
**@set_remote_ip**:*ip*<br>
**@set_remote_port**:*port*<br>

These modifiers change *every* connection opened event to use the given 
parameters.

**@parse**

This modifier converts all connection data in events into request started, 
request headers, request finished events and call connection data out events
into response started, response headers, and response finished events.

The parser used to do this is extremely simple.  It, essentially, splits the
first line on spaces into three values (the request/response line values), 
splits the next lines on : into two values (header key and value), and, when
it sees a blank line, treats the remainder of the data as the body.

At present, @parse does not support repeated connection data in or connection
data out events.  Handling those properly (also repeat parsed events) would 
require a smarter parser and handling those dumbly (join them and process as
a single block of text) was deemed more unexpected than useful.  So, if 
repeated events are present, an error will be displayed and the input 
discarded.

**@unparse**

This modifier is the opposite of parse, converting the parsed events into
connection data in and connection data out events.  It generates a single
connection data in (out) event for each set of request (response) events, even
if some of those events are repeated.

**@aggregate**<br>
**@aggregate**:*n*<br>
**@aggregate**:*distribution*:*parameters*

Aggregates multiple connections together.  The first connection provides the
pre and post transactions events.  Subsequent transactions have their 
transactions appended to the first.  When there are no more inputs or when
the number of transactions is at least *n*, the connection is passed on.  
Note that the final connection may have less than *n* transactions, i.e.,
it will have the remainder.

If given a distrbution and distrbution parameters, the value of *n* will be
chosen at random for each output input.  Supported distrbutions are:

- uniform:*min*,*max* --- Uniform distribution from [*min*, *max*].
- binomial:*t*,*p* --- Binomial distribution of *t* trials with *p* chance of
  success.

Consumers
---------

**ironbee**:*path*

This consumer initialized an IronBee engine, loads *path* as configuration, 
and feeds all events to it.  The pre- and post- delay attributes of Events
are interpreted.

**view**
**view:id**
**view:summary**

These consumer are identical to the modifiers of the same name except that they behave as a consumer, i.e., can appear as the final chain.

**writepb**:*path* 

This consumer the Inputs to *path* in the CLIPP protobuf format.  This format
perfectly captures the Inputs.

Extending CLIPP
---------------

CLIPP is designed to be extendable via adding additional components: 
generators, modifiers, and consumers.  As a component writer, you will need to
be familiar with `input.hpp`.

All components are C++ functionals.  You will need to write the functional
(probably as a class) and then modify `clipp.cpp` to add the functional to 
the appropriate factory map (`clipp.cpp` has specific documentation for 
doing this).  If your functional can be instantiated with a single 
`std::string` argument, then this addition is easy.  Otherwise, you will also
need to write a factory functional which takes a single `std::string` argument
and returns the component functional.

All components use shared pointers to the Input class for their parameter. 
This type is called an `input_p`.  All components return a bool. 

Generators take an `input_p&` parameter.  The parameter is guaranteed to be
non-singular, i.e., non-NULL.  The generator can either reuse the Input 
pointed to or reset the parameter to a new Input.  The Generator should make
no assumptions about the value of the passed in Input.  It can be reset, via

   input = Input::Input();
   
A generator should return true if and only if it was able to generate an 
Input.  When it returns false, `clipp` will discard the Input and move on to
the next Generator.

Modifiers also take an `input_p&` parameter.  The parameter is guaranteed to
point to an incoming Input.  The modifier can modify that Input or generate a
new Input based on it and change the parameter to point to the new one.  The 
modifier should return true if processing of this Input should continue and 
false otherwise.  Returning false is useful, e.g., for filters.  If false is
returned, `clipp` will stop processing the Input and ask the Generator for the
next input.

When the generator returns false, a singular, i.e., NULL, input will be sent
through the modifier chain.  This allows modifiers to detect end-of-input
conditions and produce additional input if appropriate, e.g., for 
aggregation or reordering.  Modifiers that are not concerend with end-of-input
conditions should immediately return true when passed a singular input.  The
chain will be complete when the generator returns false and a singular input
reaches the consumer.

Consumer take a `const input_p&` parameter.  They are, however, allowed to 
modify the pointed to Input if that helps them.  The Input will be not be
read after the Consumer runs.  Consumers should return true if they were able
to process the input and false otherwise.  If a consumer returns false, 
`clipp` will emit an error message and exit.

All components should indicate error conditions by throwing standard 
exceptions.  Errors during construction will cause `clipp` to exit.  Errors
during Input processing will cause `clipp` to emit an error and move on to the
next input.  Thus, e.g., if a Consumer suffers a fatal error it should both
through an exception and arrange to return false in the future.

The Input classes provide a variety of routines to ease component writing.
For Generators, there are methods to easily add new Transactions and Events.
For Modifiers, there is a `ModifierDelegate` class and `dispatch` methods to 
visit every Event in a non-const fashion.  For Consumers, there is a
`Delegate` class and `dispatch` methods to visit every Event in a 
const-fashion, with, if desired, delays.

As functionals, components must be easily copied.  A standard technique to 
handle this (and to encapsulate implementation) is to have the functional 
classes hold only a shared pointer to a declared but not defined internal
state class.

For simple examples see:

- `raw_generator.[ch]pp`
- `view.[ch]pp`
- `connection_modifiers.[ch]pp`



