.. index:: ! transient storage

.. _transient-storage:

*****************
Transient Storage
*****************

Transient storage is another data location besides memory, storage, calldata
(and return-data and code) which was introduced alongside its respective opcodes
``TSTORE`` and ``TLOAD`` by `EIP-1153 <https://eips.ethereum.org/EIPS/eip-1153>`_.
This new data location behaves as a key-value store similar to storage with the main
difference being that data in transient storage is not permanent, but is scoped to
the current transaction only, after which it will be reset to zero. The values on
transient storage are never deserialized from storage or serialized to storage,
thus the gas cost of operating in transient storage is much cheaper,
since it doesn't require disk access.

Currently, the compiler can parse ``transient`` as a data location, however it is not
defined as a keyword of the language yet. This means that the use of ``transient``
is backwards-compatible and does not break previous code that eventually used it as an identifier.
Note however that such use of ``transient`` as a data location is only allowed for
:ref:`value type <value-types>` state variable declarations. Further support for
other types as well as code generation is intended with upcoming releases.

An expected canonical use case for transient storage is cheaper reentrancy locks,
which can be readily implemented with the opcodes as showcased next.

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity ^0.8.27;

    contract Generosity {
        mapping(address => bool) sentGifts;
        bool transient locked;

        modifier nonReentrant {
            require(!locked, "Reentrancy attempt");
            locked = true;
            _;
            // Unlocks the guard, making the pattern composable.
            // After the function exits, it can be called again, even in the same transaction.
            locked = false;
        }

        function claimGift() nonreentrant public {
            require(address(this).balance >= 1 ether);
            require(!sentGifts[msg.sender]);
            (bool success, ) = msg.sender.call{value: 1 ether}("");
            require(success);

            // In a reentrant function, doing this last would open up the vulnerability
            sentGifts[msg.sender] = true;
        }
    }

.. note::
    Given the caveats mentioned in the specification of EIP-1153, utmost care is
    recommended for more advanced use cases of transient storage to preserve
    the composability of your smart contract.