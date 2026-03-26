# MiNiFi Behave Framework

This is a test framework designed to aid in the Behavior-Driven Development (BDD) integration testing of Apache NiFi MiNiFi C++.

The framework uses Docker containers to verify the agent's behavior, both in isolation and alongside third-party components. It provides predefined steps to configure a flow, deploy various containers, and automatically verify the results.

## Prerequisites

Before running the tests, ensure your system has the following installed:
* **Python:** 3.10 or higher
* **Docker & Docker Compose:** Required for spinning up MiNiFi and third-party containers.

## Installation

The framework is packaged as a standard Python project. It is highly recommended to use a virtual environment.

1. Navigate to the root of the framework (where the `pyproject.toml` is located).
2. Set up and activate a virtual environment:
   ```bash
   python -m venv venv
   source venv/bin/activate  # On Windows use `venv\Scripts\activate`
   ```
3. Install the package and its dependencies (including `behavex`, `docker`, and `PyYAML`) in editable mode:
   ```bash
   pip install -e .
   ```

## Writing New Tests

To write additional tests, create a `features` folder within your extension's directory and describe your desired behavior using standard Gherkin syntax.

### Step Definitions

* **Common Steps:** The most common steps (e.g., creating processors, connecting relationships, checking files) are already defined in the core framework under:
  `src/minifi_behave/steps/`
* **Domain-Specific Steps:** If you need custom steps tailored to a new extension, define them inside your extension's specific `steps` folder.
  *Example:* `extensions/standard-processors/tests/features/steps/steps.py`

### Example Feature

```gherkin
Feature: File system operations are handled by the GetFile and PutFile processors
  In order to store and access data on the local file system
  As a user of MiNiFi
  I need to have GetFile and PutFile processors

  Scenario: Get and put operations run in a simple flow
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 10 seconds
```

## Running the Tests

Make sure your virtual environment is activated, then run:

```bash
# Run all tests
behavex 

# Run a specific feature file
behavex extensions/standard-processors/tests/features/file_system_operations.feature

# Run tests with a specific tag (if you tag your scenarios)
behavex -t @my_tag
```