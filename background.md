## Diffusion3D - 3D Diffusion in Structured Environments under OpenACC

Copyright: the project contributors, June 2019.

### Background Information

#### 1.1) Introduction

Diffusion is a fundamentally important microscopic transport process yielding effects that are of great significance within many branches of science and engineering. At the level of an individual particle (which might be anything from the size of an electron, ion or atom up to the size of aggregated macro-molecules) the process is nothing more than a random walk: a sequence of motion paths punctuated by collisions where the direction of motion changes. Remarkably, this haphazard microscopic behavior "disappears" when large populations of particles are considered at the macroscopic scale. The behaviour of the whole is accurately modelled using the same elegant mathematics that define the Normal or Gaussian distribution.

When a problem can be expressed such that an analytic equation (e.g. that of the Gaussian distribution) is applicable, then accurate solutions can be determined with great ease. For diffusion problems this requires complete uniformity in space and time: variation in the conditions controlling the diffusion process are not accomodated within the analytic equation. This unfortunately means that in practice, when variations in physical structure, temperature or other factors are essential to the problem formulation, analytic methods are of very limited use.

A practical approach to solving non-trivial diffusion problems is provided by discretisation: dividing space into a set of finite locations and applying rules that govern behaviour within and interaction between locations. Due to its simplicity, uniform subdivision of space forming a lattice of sites (point-locations) is a popular approach. Typically implemented in software as an array of floating point numbers, each element of the array (lattice point) relates to a spatial location and each element value (i.e. floating point number) represents the local quantity of a diffusant (i.e. particles undergoing diffusion). The diffusion process is simulated in a number of software iterations (each representing a discrete time step) with all array elements being updated once within each iteration before the next can begin. The update of each array element is achieved through a discrete spatial operator that uses the values of the element and its nearest neighbours to determine the new element value according to mathematical rules. The physical process represented is the simultaneous movement of some diffusant into each lattice site (a software array element) from its neighbouring sites. The quantity of diffusant moving from a neighbour is a fraction of the quantity within that neighbour site: this fraction is known as the diffusion coefficient. In simple cases the same fraction is applied to every site within every time step i.e. the diffusion coefficient is constant, resulting in uniform diffusion. Viewed as a whole, this is a gradual local spatial averaging process, out of which the correct pattern (the Gaussian distribution) emerges. By varying the discrete spatial operator per lattice site (representing local structure or other changing conditions) it is possible to simulate complex structure and behaviour that is difficult or impossible to describe analytically.


#### 1.2) Correctness and Accuracy

The fundamental measure of correctness in any physical model is mass conservation: all quantities should be accounted for. In a closed system without any addition or removal of diffusant, the total quantity is expected to remain the same following every iteration. In practice this is difficult to achieve with perfect precision, due to the way that computing machinery commonly represents and handles numbers. However a high level of precision (better than that of typical laboratory measurements, for example) is achievable with careful implementation. The great fundamental rule of numerical diffusion is the constraint of diffusion coefficients to maintain conservation - see [Crank, 1975][Crank75] for details. Less fundamental but equally important, software implementation of a numerical diffusion scheme introduces other issues that need careful consideration.

Firstly, the use of floating point numbers (having finite range and precision due to their finite storage size) means accuracy is limited. This is significant because of the fractional decrease in the local quantity of diffusant as it spreads across lattice sites within the computational domain (i.e. elements of the array in software). If we repeatedly take half of some initial quantity to a new location, and then half of that to a new location and so on, then the Nth new location will contain 0.5^N of the original quantity. If N is 128 then the quantity at that location is 2.9E-39 of the original - a number smaller than allowed by IEEE-754 single precision floating point representation. A numerical "trick" such as scaling up the original quantity to the largest available number allows this size to be doubled, but then what if the diffusion coefficient is very small? In general it is important to use double precision floating point; this affords accuracy for lattice sizes measuring thousands of elements rather than hundreds.

The second major concern is how to update array elements (representing sites in a lattice) using temporally consistent local neighbour information. If the update is done "in place" on a single array then the first site once updated is an iteration (i.e. time step) ahead of it neighbours. When these neighbours are themselves updated, the local information they need is no longer all from the same iteration. This violates underlying principles used to formulate efficient discrete spatial operators and the consequences are disastrous. This problem can be avoided by using two arrays, each able to represent the simulated lattice, and processing from one into the other at each iteration. Thus if initialise array A at iteration zero and then process by reading all data from A but writing only to B (which then holds iteration one) then we avoid any temporal inconsistency within local spatial operators. Subsequently iteration two reads data from B and writes into A, then A back to B for iteration three and so on. This approach is known as "_double buffering_" and is often the first choice wherever temporal consistency has to be maintained.

Although clever alternatives such as the "_rolling buffer_" use *much* less memory, the simplicity of the double buffer allows higher performance execution on parallel vector hardware such as a GPU.


#### 1.3) Performance and Complexity

Big => slow
Parallel => faster


#### Bibliography & References

[Crank75]: Crank, J. (1975) The Mathematics of Diffusion (2nd Edn.), Oxford University Press, ISBN0198533446.


