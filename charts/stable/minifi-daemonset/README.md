# Apache MiNiFi Daemonset

This chart deploys a Daemonset of Apache MiNiFi using Flow configuration provided in values.

## Configuration

There are two methods of deploying Flow configuration in this chart: Values or Files. Using the Values method is most appropriate in quick test scenarios where configuration doesn't need to persist. The File method is best used when expanding this chart in version control for specific use-cases.

Precedence: When configuration is provided via Values, Files will be ignored.

### With Values

MiNiFi configuration can be provided using multiline values.

There are two values to use:

**configuration.flow**
This field takes the Flow configuration to be run. It expects a multiline string:
```
configuration:
  flow: |
    your
    flow
    config
    here
```

**configuration.properties**
This field takes the MiNiFi properties configuration. It expects a multiline string:
```
configuration:
  properties: |
    your
    properties
    here
```


### With Files

MiNiFi configuration can be provided using files within the chart. These files must exist within the root path of the chart, i.e. `minifi-daemonset/files/config.yaml`.

Files must be referenced in the values file:

**configFile.flow**
Specifies the file which contains the Flow definition.
```
configFile:
  flow: somedir/config.yaml
```

**configFile.properties**
Specifies the file which contains the MiNiFi properties definitions.
```
configFile:
  properties: somedir/properties.yaml
```
