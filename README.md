# Bluetooth Low Energy

Bluetooth Low Energy device support for [ossia score](https://ossia.io).

Bluetooth availability is checked asynchronously when the plug-in is created.
The result is shared across factory queries, including repeated application
instances in static builds; querying it must not consume the initialization result.
The score tree's `test_regression_minimal_app_twice` exercises this lifecycle.
