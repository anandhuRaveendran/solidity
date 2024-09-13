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

        function claimGift() nonReentrant public {
            require(address(this).balance >= 1 ether);
            require(!sentGifts[msg.sender]);
            (bool success, ) = msg.sender.call{value: 1 ether}("");
            require(success);

            // In a reentrant function, doing this last would open up the vulnerability
            sentGifts[msg.sender] = true;
        }
    }

*********************************************************************
Composability of Smart Contracts and the Caveats of Transient Storage
*********************************************************************

Given the caveats mentioned in the specification of EIP-1153,
in order to preserve the composability of your smart contract,
utmost care is recommended for more advanced use cases of transient storage.

For smart contracts, composability is a very important design to achieve a self-contained behaviour,
such that multiple calls into individual smart contracts can be composed to more complex applications.
So far the EVM largely guaranteed composable behaviour, since multiple calls into a smart contract
within a complex transaction are virtually indistinguishable from multiple calls to the contract
stretched over several transactions. However, transient storage allows a violation to this principle
and incorrect use may lead to complex bugs that only surface when used across several calls.

Let's illustrate the problem with a simple example:

.. code-block:: solidity

    // SPDX-License-Identifier: GPL-3.0
    pragma solidity ^0.8.27;

    contract MulService {
        uint transient multiplier;
        function setMultiplier(uint mul) external {
            multiplier = mul;
        }

        function multiply(uint value) external view returns (uint) {
            return value * multiplier;
        }
    }

If the example used memory or storage to store the multiplier, it would be fully composable.
It would not matter whether you split the sequence into separate transactions or grouped them in some way.
You would always get the same result. This enables use cases such as batching calls from multiple transactions
together to reduce gas costs. Transient storage potentially breaks such use cases since composability can no longer be taken for granted.

Note however, that the lack of composability is not an inherent property of transient storage.
It could have been preserved if the rules for resetting its content were slightly adjusted.
Currently the clearing happens for all contracts at the same time, when the transaction ends.
If instead it was cleared for a contract as soon as no function belonging to it remained active
on the call stack (which could mean multiple resets per transaction), the issue would disappear.
In the example above it would mean clearing transient storage after each of the calls.

As another example, since transient storage is constructed as a relatively cheap key-value store,
a smart contract author may be tempted to use transient storage as a replacement for in-memory mappings
without keeping track of the modified keys in the mapping and thereby without clearing the mapping
at the end of the call. This, however, can easily lead to unexpected behaviour in complex transactions,
in which values set by a previous call into the contract within the same transaction remain.

The use of transient storage for reentrancy locks that are cleared at the end of the call frame
into the contract, is safe. However, be sure to resist the temptation to save the 100 gas used
for resetting the reentrancy lock, since failing to do so, will restrict your contract to
only one call within a transaction, preventing its use in complex composed transactions,
which have been a cornerstone for complex applications on chain.

It is recommend to generally always clear transient storage completely at the end of a call
into your smart contract to avoid these kinds of issues and to simplify
the analysis of the behaviour of your contract within complex transactions.