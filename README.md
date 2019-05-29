# Diffusion3D
3D Diffusion under OpenACC

---
Copyright: the project contributors, January-June 2019.

Background

Diffusion is a fundamentally important microscopic transport process yielding effects that are of great significance within many branches of science and engineering. At the level of an individual particle (which might be anything from the size of an electron, ion or atom up to the size of aggregated macro-molecules) the process is nothing more than a random walk: a sequence of motion paths punctuated by collisions where the direction of motion changes. Remarkably, this haphazard microscopic behavior "disappears" when large populations of particles are considered at the macroscopic scale. The behaviour of the whole is accurately modelled using the same elegant mathematics that define the Normal or Gaussian distribution.

When a problem can be expressed such that an analytic equation (e.g. that of the Gaussian distribution) is applicable, then accurate solutions can be determined with great ease. For diffusion problems this requires complete uniformity in space and time: variation in the conditions controlling the diffusion process are not accomodated within the analytic equation. This unfortunately means that in practice, when variations in physical structure, temperature or other factors are essential to the problem formulation, analytic methods are of little use.

A practical approach to solving non-trivial diffusion problems is provided by discretisation: dividing space into a set of finite locations and applying rules that govern behaviour within and interaction between locations.
Uniform subdivision of space forming a lattice of sites (point-locations) is an approach that is widely adopted due to the relative simplicity with which a system can be modelled in software.


Notes


---
