[![CI](https://github.com/Astronomax/vrptw-powerful-route-minimization-heuristic/actions/workflows/test.yml/badge.svg)](https://github.com/Astronomax/vrptw-powerful-route-minimization-heuristic/actions/workflows/test.yml)

# vrptw-powerful-route-minimization-heuristic

Efficient and fast implementation of the algorithm described in the following articles:
* A powerful route minimization heuristic for the vehicle routing problem with time windows, Operations Research Letters, Volume 37, Issue 5, 2009,
Pages 333-338 [[link]](https://doi.org/10.1016/j.orl.2009.04.006)
* A penalty-based edge assembly memetic algorithm for the vehicle routing problem with time windows, Computers & Operations Research, Volume 37, Issue 4, 2010, Pages 724-737 [[link]](https://doi.org/10.1016/j.cor.2009.06.022)

> The vehicle routing problem with time windows (VRPTW) is one of the most important and widely studied problems in the operations research. The objective of the
VRPTW is to minimize the number of routes (primary objective) and, in case of ties, the
total travel distance (secondary objective). Given the hierarchical objective, most of the
recent and best heuristics for the VRPTW use a two-stage approach where the number
of routes is minimized in the first stage and the total distance is then minimized in the
second stage. It has also been shown that minimizing the number of routes is sometimes
the most time consuming and challenging part of solving VRPTWs.
https://www.sintef.no/globalassets/project/vip08/abstract_nagata.pdf

This algorithm does exactly that: it minimizes the primary objective.

Various implementations that minimize the secondary objective can be attached to it. For example, the algorithm described in the following:
* Edge assembly‐based memetic algorithm for the capacitated vehicle routing problem, Networks, Volume 54, 2009 [[link]](https://doi.org/10.1002/net.20333)

Its implementation may also appear in this repository at some point, although this is not so interesting.

## Benchmark Problem Sets

* [Solomon's problem sets](https://www.sintef.no/projectweb/top/vrptw/solomon-benchmark/) (25, 50, and 100 customers)
* [Gehring & Homberger's extended benchmark](https://www.sintef.no/projectweb/top/vrptw/homberger-benchmark/) (200, 400, 600, 800, and 1000 customers)

## Usage

This implementation is just a simple command line utility. 
But it's also pretty easy to borrow and use as a library in your own project. For example, to use it in conjunction with your own implementation of the algorithm that minimizes the total travel distance (secondary objective).

Before running the cmake build process, you need to load some external submodules (for now these are only public Tarantool helper repositories). Run the following:

```console
$ git submodule update --init --recursive
```

After that, build in the standard way:

```console
$ mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo && make && cd ..
```

Then you can run the utility:
```console
$ ./build/routes --help
Usage: ./build/routes <file1> <file2> [<options>]
file1 - problem statement, file2 - where to output the solution

Options:
  --beta_correction       - Enables beta-correction mechanism.
  --log_level=<option>    - Log level: none, normal, verbose.
  --n_near=<value>        - Sets the preferred n_near.
  --k_max=<value>         - Sets the preferred k_max.
  --t_max=<value>         - Sets the preferred t_max (in secs).
  --i_rand=<value>        - Sets the preferred i_rand.
  --lower_bound=<value>   - Sets the preferred lower_bound
$ ./build/routes GehringHomberger1000/C1_10_1.TXT C1_10_1.sol --lower_bound 100 --t_max 120
```
After completion, the current directory will contain a file with the solution, the name of which you specified when starting. In this example it is "C1_10_1.sol".
