## Differences between JSON and YAML implementation

### YAML

The possible types of a `YAML::Node` are:
* Undefined
* Null
* Map
* Sequence
* Scalar

#### Undefined

The result of querying any member of `Null`, querying non-existing members of `Map`,
or non-existing indices of `Sequence`.

Note that for `Map`s string conversion applies `map[0]` could be valid, given a key `"0"`,
while for `Sequence`s string index parsing does NOT happen `seq["0"]`
will return `Undefined` even if the sequence is non-empty.

Querying or otherwise accessing an `Undefined` (other than `operator bool` or `IsDefined`) usually throws.

#### Null

The value of parsing an empty document, the value of a `Map` item with empty value,
the value of an omitted `Sequence` item.

```
key1:      # this is a Null
key2: ''   # this is a Scalar, the empty string
arr:
  - one
  -        # Null as well
  - three
```

#### Scalar

A string value, all conversions to numbers happen on the fly.

### Conversions

#### 1. `::as<std::string>`

* `Null` --> `"null"`
* `Scalar` --> the string value
* others --> throws

#### 2. `::as<YAML::Node>`

It was used in multiple places, it seems to throw on `Undefined` and return itself for all
other types.

### JSON 

In contrast to these JSON has real bools, numbers, strings, there is no string-to-int
conversion happening.
