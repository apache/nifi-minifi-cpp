<!--
  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->
# MiNiFi - C++ Expression Language

Apache NiFi - MiNiFi - C++ supports a subset of the [Apache NiFi Expression
Language](https://nifi.apache.org/docs/nifi-docs/html/expression-language-guide.html)
(EL).  EL is a tiny DSL enabling processor property values to be computed
dynamically according to contextual information such as FlowFile attributes.
Dynamic values may be manipulated by a number of functions supported by EL,
including boolean logic, string manipulation, encoding/decoding, searching,
mathematical operators, date manipulation, type coercion, and more.
Processors/properties supporting EL are marked in the [processors
documentation](PROCESSORS.md).

## Overview

All data in Apache NiFi is represented by an abstraction called a FlowFile. A
FlowFile comprises two major pieces: content and attributes. The content
portion of the FlowFile represents the data on which to operate. For instance,
if a file is picked up from a local file system using the GetFile Processor,
the contents of the file will become the contents of the FlowFile.

The attributes portion of the FlowFile represents information about the data
itself, or metadata. Attributes are key-value pairs that represent what is
known about the data as well as information that is useful for routing and
processing the data appropriately. Keeping with the example of a file that is
picked up from a local file system, the FlowFile would have an attribute called
`filename` that reflected the name of the file on the file system.
Additionally, the FlowFile will have a `path` attribute that reflects the
directory on the file system that this file lived in. The FlowFile will also
have an attribute named `uuid`, which is a unique identifier for this FlowFile.
For complete listing of the core attributes check out the FlowFile section of
the Apache NiFi Developer’s Guide.

However, placing these attributes on a FlowFile do not provide much benefit if
the user is unable to make use of them. The NiFi Expression Language provides
the ability to reference these attributes, compare them to other values, and
manipulate their values.

## Structure of a NiFi Expression

The NiFi Expression Language always begins with the start delimiter `${` and
ends with the end delimiter `}`. Between the start and end delimiters is the
text of the Expression itself. In its most basic form, the Expression can
consist of just an attribute name. For example, `${filename}` will return the
value of the `filename` attribute.

In a slightly more complex example, we can instead return a manipulation of
this value. We can, for example, return an all upper-case version of the
filename by calling the `toUpper` function: `${filename:toUpper()}`. In this
case, we reference the `filename` attribute and then manipulate this value by
using the `toUpper` function. A function call consists of 5 elements. First,
there is a function call delimiter `:`. Second is the name of the function --in
this case, `toUpper`. Next is an open parenthesis (`(`), followed by the
function arguments.  The arguments necessary are dependent upon which function
is being called. In this example, we are using the `toUpper` function, which
does not have any arguments, so this element is omitted. Finally, the closing
parenthesis (`)`) indicates the end of the function call. There are many
different functions that are supported by the Expression Language to achieve
many different goals. Some functions provide String (text) manipulation, such
as the `toUpper` function.  Others, such as the equals and matches functions,
provide comparison functionality. Functions also exist for manipulating dates
and times and for performing mathematical operations. Each of these functions
is described below, in the Functions section, with an explanation of what the
function does, the arguments that it requires, and the type of information that
it returns.

When we perform a function call on an attribute, as above, we refer to the
attribute as the subject of the function, as the attribute is the entity on
which the function is operating. We can then chain together multiple function
calls, where the return value of the first function becomes the subject of the
second function and its return value becomes the subject of the third function
and so on. Continuing with our example, we can chain together multiple
functions by using an expression similar to
`${filename:toUpper():equals('HELLO.TXT')}`.  There is no limit to the number
of functions that can be chained together.

Any FlowFile attribute can be referenced using the Expression Language.
However, if the attribute name contains a special character, the attribute
name must be escaped by quoting it. The following characters are each
considered special characters:

- `$` (dollar sign)
- `|` (pipe)
- `{` (open brace)
- `}` (close brace)
- `(` (open parenthesis)
- `)` (close parenthesis)
- `[` (open bracket)
- `]` (close bracket)
- `,` (comma)
- `:` (colon)
- `;` (semicolon)
- `/` (forward slash)
- `*` (asterisk)
- `'` (single quote)
- ` ` (space)
- `\t` (tab)
- `\r` (carriage return)
- `\n` (new-line)

Additionally, a number is considered a special character if it is the first
character of the attribute name. If any of these special characters is present
in an attribute is quoted by using either single or double quotes. The
Expression Language allows single quotes and double quotes to be used
interchangeably. For example, the following can be used to escape an attribute
named my attribute: `${"my attribute"}` or `${'my attribute'}`.

In this example, the value to be returned is the value of the "my attribute"
value, if it exists. If that attribute does not exist, the Expression Language
will then look for a System Environment Variable named "my attribute." Finally,
if none of these exists, the Expression Language will return a null value.

There also exist some functions that expect to have no subject. These functions
are invoked simply by calling the function at the beginning of the Expression,
such as `${hostname()}`. These functions can then be changed together, as well.
For example, `${hostname():toUpper()}`. Attempting to evaluate the function with
subject will result in an error. In the Functions section below, these
functions will clearly indicate in their descriptions that they do not require
a subject.

Often times, we will need to compare the values of two different attributes to
each other. We are able to accomplish this by using embedded Expressions. We
can, for example, check if the filename attribute is the same as the uuid
attribute: `${filename:equals( ${uuid} )}`. Notice here, also, that we have a
space between the opening parenthesis for the equals method and the embedded
Expression. This is not necessary and does not affect how the Expression is
evaluated in any way. Rather, it is intended to make the Expression easier to
read. White space is ignored by the Expression Language between delimiters.
Therefore, we can use the Expression `${ filename : equals(${ uuid}) }` or
`${filename:equals(${uuid})}` and both Expressions mean the same thing. We
cannot, however, use `${file name:equals(${uuid})}`, because this results in
file and name being interpreted as different tokens, rather than a single
token, filename.

## Supported Features

### Boolean Logic

- [`isNull`](#isnull)
- [`notNull`](#notnull)
- [`isEmpty`](#isempty)
- [`equals`](#equals)
- [`equalsIgnoreCase`](#equalsignorecase)
- [`gt`](#gt)
- [`ge`](#ge)
- [`lt`](#lt)
- [`le`](#le)
- [`and`](#and)
- [`or`](#or)
- [`not`](#not)
- [`ifElse`](#ifelse)

### String Manipulation

- [`toUpper`](#toupper)
- [`toLower`](#tolower)
- [`substring`](#substring)
- [`substringBefore`](#substringbefore)
- [`substringBeforeLast`](#substringbeforelast)
- [`substringAfter`](#substringafter)
- [`substringAfterLast`](#substringafterlast)
- [`replace`](#replace)
- [`replaceFirst`](#replacefirst)
- [`replaceAll`](#replaceall)
- [`replaceNull`](#replacenull)
- [`replaceEmpty`](#replaceempty)
- [`trim`](#trim)
- [`append`](#append)
- [`prepend`](#prepend)
- [`length`](#length)

### Mathematical Operations and Numeric Manipulation

- [`plus`](#plus)
- [`minus`](#minus)
- [`multiply`](#multiply)
- [`divide`](#divide)
- [`mod`](#mod)
- [`toRadix`](#toradix)
- [`fromRadix`](#fromradix)
- [`random`](#random)

### Searching

- [`startsWith`](#startswith)
- [`endsWith`](#endswith)
- [`contains`](#contains)
- [`in`](#in)
- [`matches`](#matches)
- [`find`](#find)
- [`indexOf`](#indexof)
- [`lastIndexOf`](#lastindexof)

### Encode/Decode Functions

- [`escapeJson`](#escapejson)
- [`unescapeJson`](#unescapejson)
- [`escapeXml`](#escapexml)
- [`escapeCsv`](#escapecsv)
- [`unescapeXml`](#unescapexml)
- [`unescapeCsv`](#unescapecsv)

## Planned Features

### String Manipulation

- `getDelimitedField`

### Searching

- `jsonPath`

### Encode/Decode Functions

- `escapeHtml3`
- `escapeHtml4`
- `unescapeHtml3`
- `unescapeHtml4`
- `urlEncode`
- `urlDecode`
- `base64Encode`
- `base64Decode`

### Date Manipulation

- `format`
- `toDate`
- `now`

### Subjectless Functions

- `ip`
- `hostname`
- `UUID`
- `nextInt`
- `literal`
- `getStateValue`

### Evaluating Multiple Attributes

- `anyAttribute`
- `allAttributes`
- `anyMatchingAttribute`
- `allMatchingAttributes`
- `anyDelineatedValue`
- `allDelineatedValues`
- `join`
- `count`

## Unsupported Features

The following EL features are currently not supported, and no support is
planned due to language/environment (Java vs. C++) differences:

### Mathematical Operations and Numeric Manipulation

- `math`

## Boolean Logic

One of the most powerful features of the Expression Language is the ability to
compare an attribute value against some other value. This is used often, for
example, to configure how a Processor should route data. The following
functions are used for performing boolean logic, such as comparing two values.
Each of these functions are designed to work on values of type Boolean.

### isNull

**Description**: The `isNull` function returns `true` if the subject is `null`,
`false` otherwise. This is typically used to determine if an attribute exists.

**Subject Type**: Any

**Arguments**: No arguments

**Return Type**: Boolean

**Examples**: `${filename:isNull()}` returns true if the `filename` attribute
does not exist. It returns `false` if the attribute exists.

### notNull

**Description**: The `notNull` function returns the opposite value of the
`isNull` function. That is, it will return `true` if the subject exists and
`false` otherwise.

**Subject Type**: Any

**Arguments**: No arguments

**Return Type**: Boolean

**Examples**: `${filename:notNull()}` returns `true` if the `filename`
attribute exists. It returns `false` if the attribute does not exist.

### isEmpty

**Description**: The `isEmpty` function returns `true` if the Subject is
`null`, does not contain any characters or contains only white-space (new line,
carriage return, space, tab), `false` otherwise.

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: Boolean

**Examples**: `${filename:isEmpty()}` returns `true` if the `filename`
attribute does not exist or contains only white space. `${literal("
"):isEmpty()}` returns `true` as well as `${literal(""):isEmpty()}`.

### equals

**Description**: The `equals` function is very widely used and determines if
its subject is equal to another String value. Note that the equals function
performs a direct comparison of two String values. Take care not to confuse
this function with the `matches` function, which evaluates its subject against
a Regular Expression.

**Subject Type**: Any

**Arguments**: 

| Argument | Description |
| - | - |
| value | The value to compare the Subject to. Must be same type as the Subject. |

**Return Type**: Boolean

**Examples**: We can check if the `filename` of a FlowFile is `hello.txt` by
using the expression `${filename:equals('hello.txt')}`, or we could check if
the value of the attribute `hello` is equal to the value of the `filename`
attribute: `${hello:equals( ${filename} )}`.

### equalsIgnoreCase

**Description**: Similar to the equals function, the `equalsIgnoreCase`
function compares its subject against a String value but returns `true` if the
two values differ only by case (upper case vs. lower case).

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| value | The value to compare the Subject to. |

**Return Type**: Boolean

**Examples**: `${filename:equalsIgnoreCase('hello.txt')}` will evaluate to
`true` if filename is equal to `hello.txt` or `HELLO.TXT` or `HeLLo.TxT`.

### gt

**Description**: The `gt` function is used for numeric comparison and returns
`true` if the subject is Greater Than its argument. If either the subject or
the argument cannot be coerced into a Number, this function returns `false`.

**Subject Type**: Number

**Arguments**: 

| Argument | Description |
| - | - |
| value | The number to compare the Subject to. |

**Return Type**: Boolean

**Examples**: `${fileSize:gt( 1024 )}` will return `true` if the size of the
FlowFile’s content is more than 1 kilobyte (1024 bytes). Otherwise, it will
return `false`.

### ge

**Description**: The `ge` function is used for numeric comparison and returns
`true` if the subject is Greater Than Or Equal To its argument. If either the
subject or the argument cannot be coerced into a Number, this function returns
`false`.

**Subject Type**: Number

**Arguments**: 

| Argument | Description |
| - | - |
| value | The number to compare the Subject to. |

**Return Type**: Boolean

**Examples**: `${fileSize:ge( 1024 )}` will return `true` if the size of the
FlowFile’s content is at least ( is greater than or equal to) 1 kilobyte (1024
bytes). Otherwise, it will return `false`.

### lt

**Description**: The `lt` function is used for numeric comparison and returns
`true` if the subject is Less Than its argument. If either the subject or the
argument cannot be coerced into a Number, this function returns `false`.

**Subject Type**: Number

**Arguments**: 

| Argument | Description |
| - | - |
| value | The number to compare the Subject to. |

**Return Type**: Boolean

**Examples**: `${fileSize:lt( 1048576 )}` will return `true` if the size of the
FlowFile’s content is less than 1 megabyte (1048576 bytes). Otherwise, it will
return `false`.

### le

**Description**: The `le` function is used for numeric comparison and returns
`true` if the subject is Less Than Or Equal To its argument. If either the
subject or the argument cannot be coerced into a Number, this function returns
`false`.

**Subject Type**: Number

**Arguments**: 

| Argument | Description |
| - | - |
| value | The number to compare the Subject to. |

**Return Type**: Boolean

**Examples**: `${fileSize:le( 1048576 )}` will return `true` if the size of the
FlowFile’s content is at most (less than or equal to) 1 megabyte (1048576
bytes). Otherwise, it will return `false`.

### and

**Description**: The `and` function takes as a single argument a Boolean value
and returns `true` if both the Subject and the argument are `true`. If either
the subject or the argument is `false` or cannot be coerced into a Boolean, the
function returns `false`. Typically, this is used with an embedded Expression
as the argument.

**Subject Type**: Boolean

**Arguments**: 

| Argument | Description |
| - | - |
| condition | The right-hand-side of the `and` Expression |

**Return Type**: Boolean

**Examples**: We can check if the filename is both all lower-case and has at
least 5 characters by using the Expression

```
${filename:toLower():equals( ${filename} ):and(
	${filename:length():ge(5)}
)}
```

### or

**Description**: The `or` function takes as a single argument a Boolean value
and returns `true` if either the Subject or the argument is `true`. If both the
subject and the argument are `false`, the function returns `false`. If either
the Subject or the argument cannot be coerced into a Boolean value, this
function will return `false`.

**Subject Type**: Boolean

**Arguments**: 

| Argument | Description |
| - | - |
| condition | The right-hand-side of the `or` Expression |

**Return Type**: Boolean

**Examples**: The following example will return `true` if either the filename
has exactly 5 characters or if the filename is all lower-case.

```
${filename:toLower():equals( ${filename} ):or(
	${filename:length():equals(5)}
)}
```

### not

**Description**: The `not` function returns the negation of the Boolean value of the subject.

**Subject Type**: Boolean

**Arguments**: No arguments

**Return Type**: Boolean

**Examples**: We can invert the value of another function by using the not
function, as `${filename:equals('hello.txt'):not()}`. This will return `true`
if the filename is NOT equal to `hello.txt` and will return `false` if the
filename is `hello.txt`.

### ifElse

**Description**: Evaluates the first argument if the Subject evaluates to
`true`, or the second argument if the Subject evaluates to `false`.

**Subject Type**: Boolean

| Argument | Description |
| - | - |
| EvaluateIfTrue | The value to return if the Subject is `true` |
| EvaluateIfFalse | The value to return if the Subject is `false` |

**Return Type**: String

**Examples**:

If the `filename` attribute has the value `a brand new filename.txt`, the
`nullFilename` attribute has the value `null`, and the `bool` attribute has the
value `true`, then the following expressions will provide the following
results:

| Expression | Value |
| - | - |
| `${bool:ifElse('a','b')}` | a |
| `${literal(true):ifElse('a','b')}` | a |
| `${nullFilename:isNull():ifElse('file does not exist', 'located file')}` | file does not exist |
| `${nullFilename:ifElse('found', 'not_found')}` | not_found |
| `${filename:ifElse('found', 'not_found')}` | not_found |
| `${filename:isNull():not():ifElse('found', 'not_found')}` | found |

## String Manipulation

Each of the following functions manipulates a String in some way.

### toUpper

**Description**: This function converts the Subject into an all upper-case
String. Said another way, it replaces any lowercase letter with the uppercase
equivalent.

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**: If the `filename` attribute is `abc123.txt`, then the Expression
`${filename:toUpper()}` will return `ABC123.TXT`

### toLower

**Description**: This function converts the Subject into an all lower-case
String. Said another way, it replaces any uppercase letter with the lowercase
equivalent.

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**: If the `filename` attribute is `ABC123.TXT`, then the Expression
`${filename:toLower()}` will return `abc123.txt`

### substring

**Description**: Returns a portion of the Subject, given a starting index and
an optional ending index. If the ending index is not supplied, it will return
the portion of the Subject starting at the given 'start index' and ending at
the end of the Subject value.

The starting index and ending index are zero-based. That is, the first
character is referenced by using the value 0, not 1.

If either the starting index is or the ending index is not a number, this
function call will result in an error.

If the starting index is larger than the ending index, this function call will
result in an error.

If the starting index or the ending index is greater than the length of the
Subject or has a value less than 0, this function call will result in an error.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| starting index | The 0-based index of the first character to capture (inclusive) |
| ending index | The 0-based index of the last character to capture (exclusive) |

**Return Type**: String

**Examples**:

If we have an attribute named `filename` with the value `a brand new
filename.txt`, then the following Expressions will result in the following
values:

| Expression | Value |
| - | - |
| `${filename:substring(0,1)}` | a |
| `${filename:substring(2)}` | brand new filename.txt |
| `${filename:substring(12)}` | filename.txt |
| `${filename:substring( ${filename:length():minus(2)} )}` | xt |

### substringBefore

**Description**: Returns a portion of the Subject, starting with the first
character of the Subject and ending with the character immediately before the
first occurrence of the argument. If the argument is not present in the
Subject, the entire Subject will be returned.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| value | The String to search for in the Subject |

**Return Type**: String

**Examples**: If the `filename` attribute has the value `a brand new
filename.txt`, then the following Expressions will result in the following
values:

| Expression | Value |
| - | - |
| `${filename:substringBefore('.')}` | a brand new filename |
| `${filename:substringBefore(' ')}` | a |
| `${filename:substringBefore(' n')}` | a brand |
| `${filename:substringBefore('missing')}` | a brand new filename.txt |

### substringBeforeLast

**Description**: Returns a portion of the Subject, starting with the first
character of the Subject and ending with the character immediately before the
last occurrence of the argument. If the argument is not present in the Subject,
the entire Subject will be returned.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| value | The String to search for in the Subject |

**Return Type**: String

**Examples**: If the `filename` attribute has the value `a brand new
filename.txt`, then the following Expressions will result in the following
values:

| Expression | Value |
| - | - |
| `${filename:substringBeforeLast('.')}` | a brand new filename |
| `${filename:substringBeforeLast(' ')}` | a brand new |
| `${filename:substringBeforeLast(' n')}` | a brand |
| `${filename:substringBeforeLast('missing')}` | a brand new filename.txt |

### substringAfter

**Description**: Returns a portion of the Subject, starting with the character
immediately after the first occurrence of the argument and extending to the end
of the Subject. If the argument is not present in the Subject, the entire
Subject will be returned.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| value | The String to search for in the Subject |

**Return Type**: String

**Examples**: If the `filename` attribute has the value `a brand new
filename.txt`, then the following Expressions will result in the following
values:

| Expression | Value |
| - | - |
| `${filename:substringAfter('.')}` | txt |
| `${filename:substringAfter(' ')}` | brand new filename.txt |
| `${filename:substringAfter(' n')}` | ew filename.txt |
| `${filename:substringAfter('missing')}` | a brand new filename.txt |

### substringAfterLast

**Description**: Returns a portion of the Subject, starting with the character
immediately after the last occurrence of the argument and extending to the end
of the Subject. If the argument is not present in the Subject, the entire
Subject will be returned.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| value | The String to search for in the Subject |

**Return Type**: String

**Examples**: If the `filename` attribute has the value `a brand new
filename.txt`, then the following Expressions will result in the following
values:

| Expression | Value |
| - | - |
| `${filename:substringAfterLast('.')}` | txt |
| `${filename:substringAfterLast(' ')}` | filename.txt |
| `${filename:substringAfterLast(' n')}` | ew filename.txt |
| `${filename:substringAfterLast('missing')}` | a brand new filename.txt |

### replace

**Description**: Replaces all occurrences of one literal String within the Subject
with another String.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| Search String | The String to find within the Subject |
| Replacement | The value to replace Search String with |

**Return Type**: String

**Examples**: If the `filename` attribute has the value `a brand new
filename.txt`, then the following Expressions will provide the following
results:

| Expression | Value |
| - | - |
| `${filename:replace('.', '_')}` | a brand new filename_txt |
| `${filename:replace(' ', '.')}` | a.brand.new.filename.txt |
| `${filename:replace('XYZ', 'ZZZ')}` | a brand new filename.txt |
| `${filename:replace('filename', 'book')}` | a brand new book.txt |

### replaceFirst

**Description**: Replaces the first occurrence of one literal String or regular
expression within the Subject with another String.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| Search String | The String (literal or regular expression pattern) to find within the Subject |
| Replacement | The value to replace Search String with |

**Return Type**: String

**Examples**: If the `filename` attribute has the value `a brand new
filename.txt`, then the following Expressions will provide the following
results:

| Expression | Value |
| - | - |
| `${filename:replaceFirst('a', 'the')}` | the brand new filename.txt |
| `${filename:replaceFirst('[br]', 'g')}` | a grand new filename.txt |
| `${filename:replaceFirst('XYZ', 'ZZZ')}` | a brand new filename.txt |
| `${filename:replaceFirst('\w{8}', 'book')}` | a brand new book.txt |

### replaceAll

**Description**: The replaceAll function takes two String arguments: a literal
String or Regular Expression (NiFi uses the Java Pattern syntax), and a
replacement string. The return value is the result of substituting the
replacement string for all patterns within the Subject that match the Regular
Expression.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| Regex | he Regular Expression (in Java syntax) to match in the Subject |
| Replacement | The value to use for replacing matches in the Subject. If the regular expression argument uses Capturing Groups, back references are allowed in the replacement. |

**Return Type**: String

**Examples**: If the `filename` attribute has the value `a brand new
filename.txt`, then the following Expressions will provide the following
results:

| Expression | Value |
| - | - |
| `${filename:replaceAll('\..*', '')}` | a brand new filename |
| `${filename:replaceAll('a brand (new)', '$1')}` | new filename.txt |
| `${filename:replaceAll('XYZ', 'ZZZ')}` | a brand new filename.txt |
| `${filename:replaceAll('brand (new)', 'somewhat $1')}` | a somewhat new filename.txt |

### replaceNull

**Description**: The replaceNull function returns the argument if the Subject is
null. Otherwise, returns the Subject.

**Subject Type**: Any

**Arguments**:

| Argument | Description |
| - | - |
| Replacement | The value to return if the Subject is null. |

**Return Type**: Type of Subject if Subject is not null; else, type of Argument

**Examples**: If the attribute `filename` has the value `a brand new filename.txt` and the attribute `hello` does not exist, then the Expression `${filename:replaceNull('abc')}` will return `a brand new filename.txt`, while `${hello:replaceNull('abc')}` will return `abc`.

### replaceEmpty

**Description**: The replaceEmpty function returns the argument if the Subject is
null or if the Subject consists only of white space (new line, carriage return,
tab, space). Otherwise, returns the Subject.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| Replacement | The value to return if the Subject is null or empty. |

**Return Type**: String

**Examples**: If the attribute `filename` has the value `a brand new
filename.txt` and the attribute `hello` has the value ` `, then the Expression
`${filename:replaceEmpty('abc')}` will return `a brand new filename.txt`, while
`${hello:replaceEmpty('abc')}` will return `abc`.

### trim

**Description**: The `trim` function will remove any leading or trailing white
space from its subject.

**Subject Type**: String

**Arguments**: No Arguments

**Return Type**: String

**Examples**: If the attribute `attr` has the value " 1 2 3 ", then the
Expression `${attr:trim()}` will return the value "1 2 3".

### append

**Description**: The `append` function returns the result of appending the
argument to the value of the Subject. If the Subject is null, returns the
argument itself.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| value | The String to append to the end of the Subject |

**Return Type**: String

**Examples**: If the "filename" attribute has the value "a brand new
filename.txt", then the Expression `${filename:append('.gz')}` will return "a
brand new filename.txt.gz".

### prepend

**Description**: The `prepend` function returns the result of prepending the
argument to the value of the Subject. If the subject is null, returns the
argument itself.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| value | The String to prepend to the beginning of the Subject |

**Return Type**: String

**Examples**: If the "filename" attribute has the value "filename.txt", then
the Expression `${filename:prepend('a brand new ')}` will return "a brand new
filename.txt".

### length

**Description**: Returns the length of the Subject

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**: If the attribute "filename" has a value of "a brand new
filename.txt" and the attribute "hello" does not exist, then the Expression
`${filename:length()}` will return 24. `${hello:length()}` will return 0.

## Mathematical Operations and Numeric Manipulation

For those functions that support Decimal and Number (whole number) types, the
return value type depends on the input types. If either the subject or argument
are a Decimal then the result will be a Decimal. If both values are Numbers
then the result will be a Number. This includes Divide. This is to preserve
backwards compatibility and to not force rounding errors.

### plus

**Description**: Adds a numeric value to the Subject. If either the argument or the
Subject cannot be coerced into a Number, returns null.

**Subject Type**: Number or Decimal

**Arguments**:

| Argument | Description |
| - | - |
| Operand | The value to add to the Subject |

**Return Type**: Number or Decimal (depending on input types)

**Examples**: If the `fileSize` attribute has a value of 100, then the
Expression `${fileSize:plus(1000)}` will return the value 1100.

### minus

**Description**: Subtracts a numeric value from the Subject.

**Subject Type**: Number or Decimal

**Arguments**:

| Argument | Description |
| - | - |
| Operand | The value to subtract from the Subject |

**Return Type**: Number or Decimal (depending on input types)

**Examples**: If the `fileSize` attribute has a value of 100, then the
Expression `${fileSize:minus(100)}` will return the value 0.

### multiply

**Description**: Multiplies a numeric value by the Subject and returns the product.

**Subject Type**: Number or Decimal

**Arguments**:

| Argument | Description |
| - | - |
| Operand | The value to multiple the Subject by |

**Return Type**: Number or Decimal (depending on input types)

**Examples**: If the `fileSize` attribute has a value of 100, then the
Expression `${fileSize:multiply(1024)}` will return the value 102400.

### divide

**Description**: Divides the Subject by a numeric value and returns the result.

**Subject Type**: Number or Decimal

**Arguments**:

| Argument | Description |
| - | - |
| Operand | The value to divide the Subject by |

**Return Type**: Number or Decimal (depending on input types)

**Examples**: If the `fileSize` attribute has a value of 100, then the
Expression `${fileSize:divide(12)}` will return the value 8.

### mod

**Description**: Performs a modular division of the Subject by the argument. That
is, this function will divide the Subject by the value of the argument and
return not the quotient but rather the remainder.

**Subject Type**: Number or Decimal

**Arguments**:

| Argument | Description |
| - | - |
| Operand | The value to divide the Subject by |

**Return Type**: Number or Decimal (depending on input types)

**Examples**: If the `fileSize` attribute has a value of 100, then the
Expression `${fileSize:mod(12)}` will return the value 4.

### toRadix

**Description**: Converts the Subject from a Base 10 number to a
different Radix (or number base). An optional second argument can be used to
indicate the minimum number of characters to be used. If the converted value
has fewer than this number of characters, the number will be padded with
leading zeroes.

If a decimal is passed as the subject, it will first be converted to a whole
number and then processed.

**Subject Type**: Number

**Arguments**:

| Argument | Description |
| - | - |
| Desired Base | A Number between 2 and 36 (inclusive) |
| Padding | Optional argument that specifies the minimum number of characters in the converted output |

**Return Type**: String

**Examples**: If the `fileSize` attributes has a value of 1024, then the
following Expressions will yield the following results:

| Expression | Value |
| - | - |
| `${fileSize:toRadix(10)}` | 1024 |
| `${fileSize:toRadix(10, 1)}` | 1024 |
| `${fileSize:toRadix(10, 8)}` | 00001024 |
| `${fileSize:toRadix(16)}` | 400 |
| `${fileSize:toRadix(16, 8)}` | 00000400 |
| `${fileSize:toRadix(2)}` | 10000000000 |
| `${fileSize:toRadix(2, 16)}` | 0000010000000000 |

### fromRadix

**Description**: Converts the Subject from a specified Radix (or
number base) to a base ten whole number. The subject will converted as is,
without interpretation, and all characters must be valid for the base being
converted from. For example converting "0xFF" from hex will not work due to "x"
being a invalid hex character.

If a decimal is passed as the subject, it will first be converted to a whole
number and then processed.

**Subject Type**: String

**Arguments**:

| Argument | Description |
| - | - |
| Subject Base | A Number between 2 and 36 (inclusive) |

**Return Type**: Number

**Examples**: If the `fileSize` attributes has a value of 1234A, then the
following Expressions will yield the following results:

| Expression | Value |
| - | - |
| `${fileSize:fromRadix(11)}` | 17720 |
| `${fileSize:fromRadix(16)}` | 74570 |
| `${fileSize:fromRadix(20)}` | 177290 |

### random

**Description**: Returns a random whole number (0 to 2^63 - 1) using an insecure
random number generator.

**Subject Type**: No subject

**Arguments**: No arguments

**Return Type**: Number

**Examples**: `${random():mod(10):plus(1)}` returns random number between 1 and
10 inclusive.

## Searching

### startsWith

**Description**: Returns `true` if the Subject starts with the String provided
as the argument, `false` otherwise.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| value | The value to search for |

**Return Type**: Boolean

**Examples**:

If the "filename" attribute has the value "a brand new filename.txt", then the
Expression `${filename:startsWith('a brand')}` will return `true`.
`${filename:startsWith('A BRAND')}` will return `false`.
`${filename:toUpper():startsWith('A BRAND')}` returns `true`.

### endsWith

**Description**: Returns `true` if the Subject ends with the String provided as
the argument, `false` otherwise.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| value | The value to search for |

**Return Type**: Boolean

**Examples**:

If the "filename" attribute has the value "a brand new filename.txt", then the
Expression `${filename:endsWith('txt')}` will return `true`.
`${filename:endsWith('TXT')}` will return `false`.
`${filename:toUpper():endsWith('TXT')}` returns `true`.

### contains

**Description**: Returns `true` if the Subject contains the value of the
argument anywhere in the value.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| value | The value to search for |

**Return Type**: Boolean

**Examples**:

If the "filename" attribute has the value "a brand new filename.txt", then the
Expression `${filename:contains('new')}` will return `true`.
`${filename:contains('NEW')}` will return `false`.
`${filename:toUpper():contains('NEW')}` returns `true`.

### in

**Description**: Returns `true` if the Subject is matching one of the provided
arguments.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| value1 | First possible matching value |
| valueN | Nth possible matching value |

**Return Type**: Boolean

**Examples**:

If the "myEnum" attribute has the value "JOHN", then the Expression
`${myEnum:in("PAUL", "JOHN", "MIKE")}` will return `true`. `${myEnum:in("RED",
"GREEN", "BLUE")}` will return `false`.

### find

**Description**: Returns `true` if the Subject contains any sequence of
characters that matches the Regular Expression provided by the argument.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| Regex | The Regular Expression to match against the Subject |

**Return Type**: Boolean

**Examples**:

If the `filename` attribute has the value "a brand new filename.txt", then the
following Expressions will provide the following results:

| Expression | Value |
| - | - |
| `${filename:find('a [Bb]rand [Nn]ew')}` | `true` |
| `${filename:find('Brand.*')}` | `false` |
| `${filename:find('brand')}` | `true` |

### matches

**Description**: Returns `true` if the Subject exactly matches the Regular Expression provided by the argument.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| Regex | The Regular Expression to match against the Subject |

**Return Type**: Boolean

**Examples**:

If the `filename` attribute has the value "a brand new filename.txt", then the
following Expressions will provide the following results:

| Expression | Value |
| - | - |
| `${filename:matches('a.*txt')}` | `true` |
| `${filename:matches('brand')}` | `false` |
| `${filename:matches('.brand.')}` | `true` |

### indexOf

**Description**: Returns the index of the first character in the Subject that
matches the String value provided as an argument. If the argument is found
multiple times within the Subject, the value returned is the starting index of
the **first** occurrence. If the argument cannot be found in the Subject,
returns `-1`. The index is zero-based. This means that if the search string is
found at the beginning of the Subject, the value returned will be `0`, not `1`.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| value | The value to search for in the Subject |

**Return Type**: Boolean

**Examples**:

If the `filename` attribute has the value "a brand new filename.txt", then the
following Expressions will provide the following results:

| Expression | Value |
| - | - |
| `${filename:indexOf('a.*txt')}` | `-1` |
| `${filename:indexOf('.')}` | `20` |
| `${filename:indexOf('a')}` | `0` |
| `${filename:indexOf(' ')}` | `1` |

### lastIndexOf

**Description**: Returns the index of the first character in the Subject that
matches the String value provided as an argument. If the argument is found
multiple times within the Subject, the value returned is the starting index of
the **last** occurrence. If the argument cannot be found in the Subject,
returns `-1`. The index is zero-based. This means that if the search string is
found at the beginning of the Subject, the value returned will be `0`, not `1`.

**Subject Type**: String

**Arguments**: 

| Argument | Description |
| - | - |
| value | The value to search for in the Subject |

**Return Type**: Boolean

**Examples**:

If the `filename` attribute has the value "a brand new filename.txt", then the
following Expressions will provide the following results:

| Expression | Value |
| - | - |
| `${filename:lastIndexOf('a.*txt')}` | `-1` |
| `${filename:lastIndexOf('.')}` | `20` |
| `${filename:lastIndexOf('a')}` | `17` |
| `${filename:lastIndexOf(' ')}` | `11` |

### escapeJson

**Description**: This function prepares the Subject to be inserted into JSON
document by escaping the characters in the String using Json String rules. The
function correctly escapes quotes and control-chars (tab, backslash, cr, ff,
etc.)

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**:

If the "message" attribute is 'This is a "test!"', then the Expression
`${message:escapeJson()}` will return 'This is a \"test!\"'

### unescapeJson

**Description**: This function unescapes any Json literals found in the String.

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**:

If the "message" attribute is 'This is a \"test!\"', then the Expression
`${message:unescapeJson()}` will return 'This is a "test!"'

### escapeXml

**Description**: This function prepares the Subject to be inserted into XML
document by escaping the characters in a String using XML entities. The
function correctly escapes quotes, apostrophe, ampersand, `<`, `>` and
control-chars.

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**:

If the "message" attribute is `Zero > One < \"two!\" & 'true'`, then the
Expression `${message:escapeXml()}` will return `Zero &gt; One &lt;
&quot;two!&quot; &amp; &apos;true&apos;`

### unescapeXml

**Description**: This function unescapes a string containing XML entity escapes
to a string containing the actual Unicode characters corresponding to the
escapes. Supports only the five basic XML entities (gt, lt, quot, amp, apos).

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**:

If the "message" attribute is `Zero &gt; One &lt; &quot;two!&quot; &amp;
&apos;true&apos;`, then the Expression `${message:escapeXml()}` will return
`Zero > One < \"two!\" & 'true'`

### escapeCsv

**Description**: This function prepares the Subject to be inserted into CSV
document by escaping the characters in a String using the rules in RFC 4180.
The function correctly escapes quotes and surround the string in quotes if
needed.

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**:

If the "message" attribute is `Zero > One < "two!" & 'true'`, then the
Expression `${message:escapeCsv()}` will return `"Zero > One < ""two!"" &
'true'"`

### unescapeCsv

**Description**: This function unescapes a String from a CSV document according
to the rules of RFC 4180

**Subject Type**: String

**Arguments**: No arguments

**Return Type**: String

**Examples**:

If the "message" attribute is `"Zero > One < ""two!"" & 'true'"`, then the
Expression `${message:escapeCsv()}` will return `Zero > One < "two!" & 'true'`
