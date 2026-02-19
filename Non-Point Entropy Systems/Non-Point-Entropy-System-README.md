# Non-Point-Entropy-System



NPES is a variable length entropy generation system. It makes non deterministic digit strings through "residuum" filtered probability collapse. Regular RNGs make fixed length values. NPES generates strings where the length itself is also non deterministic. Thus creating a much richer entropy space.



###### Terminology



Residuum - The implicit boundary structure created by base *b* modular arithmetic. It's the ambient geometry that emerges from the math.

Flush Rate - The modulus parameter /s applied during generation. This controls the residuum's "width" and directly changes the mean output length.

Collapse - When a digit position fails to manifest during generation its a "non point".

Manifestation - When a digit position successfully passes through the residuum filter and becomes part of the output string.



###### Key Characteristics



Variable output length - Seeds range from 0 to MAX\_ROLLS digits

Residuum-based filtering - Mathematical base determines acceptance criteria

High throughput - ~33 million seeds/second (256-digit, base 10 configuration)

Scalable - Tested from 256 to 2048 digits

Multi base capable - Operates in any numerical base (positive, negative, and non-standard)



##### Core Algorithm



###### Generation Process



*c*

int generate\_seed(uint64\_t\* rng\_state, int flush\_rate, int base) {

&nbsp;   int digit\_count = 0;

&nbsp;   int abs\_base = (base < 0) ? -base : base;

&nbsp;   

&nbsp;   for (int slot = 0; slot < MAX\_ROLLS; slot++) {

&nbsp;       int roll = splitmix64(rng\_state) % flush\_rate;

&nbsp;       

&nbsp;       if (roll >= abs\_base) {

&nbsp;           // Position collapses - does not manifest

&nbsp;           continue;

&nbsp;       }

&nbsp;       

&nbsp;       // Position manifests as a digit

&nbsp;       digit\_count++;

&nbsp;   }

&nbsp;   

&nbsp;   // For negative bases, apply sign flip

&nbsp;   if (base < 0) {

&nbsp;       if (splitmix64(rng\_state) \& 1) {

&nbsp;           digit\_count = -digit\_count;

&nbsp;       }

&nbsp;   }

&nbsp;   

&nbsp;   return digit\_count;

}



###### Step-by-Step Execution



1\. Initialize: Set digit\_count = 0

2\. For each of MAX\_ROLLS positions:

&nbsp;  Generate random value: roll = RNG() % flush\_rate

&nbsp;  Apply residuum filter: if (roll >= |base|) continue

&nbsp;  If passed: increment digit\_count

3\. Sign handling (negative bases only): 50% chance to negate length

4\. Return: Final digit count (signed for negative bases)



Note

The residuum operates on possibility space BEFORE manifestation.  

Don't count the positions until they manifest. The collapsed positions never existed!! They're possibilities that didn't pass through the residuum filter. No matter the flush rate, the possibility exists.  



##### Parameters



###### MAX\_ROLLS



The maximum number of digit positions attempted / The upper bound on the output length.

Typical values - 256, 512, 1024, 2048, etc...



###### BASE



The number base that defines the residuum structure. This determines the acceptance thresholds and thus the radix boundaries themselves. 

Typical values - 10, 12, -2, -3, -4...



###### flush\_rate



The modulus applied to raw entropy that sets the residuum "width" / mean output length

Typical values - 11 to 111 (120 for deep sweep) for base-10

&nbsp;

###### Multi-Base Operation and Parallel Execution



NPES-256 supports simultaneous generation in multiple bases using thread based parallelization.



example

*c*

// Even threads: Base 10

// Odd threads: Base -3

thread\_data\[i].base = (i % 2 == 0) ? 10 : -3;



Each base operates with an independent RNG state, separate counting arrays, and isolated residuum filters.

The bases only share CPU execution, L3 cache, and the wall clock time.



##### Observed Phenomena



Despite being independant at the code level, parallel multi base executions exhibit cross base boundary interference, coupled length distributions, and even non local statistical correlations.



**Critical - Never Round**

Nature doesn't round. Neither should this system. A photon doesnt emit 99.98375% of its light...

All calculations must preserve integer precision until final output. Use integer arithmetic throughout.



-O3: Aggressive optimization (required for performance)

-static: Static linking (required for proper exe)

-pthread: POSIX threads support

-lm: Math library



Without -static, the binary WILL compile successfully but it WILL crash silently when you execute it.

**This isn't optional.**







