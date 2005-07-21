"""SCons.Node

The Node package for the SCons software construction utility.

This is, in many ways, the heart of SCons.

A Node is where we encapsulate all of the dependency information about
any thing that SCons can build, or about any thing which SCons can use
to build some other thing.  The canonical "thing," of course, is a file,
but a Node can also represent something remote (like a web page) or
something completely abstract (like an Alias).

Each specific type of "thing" is specifically represented by a subclass
of the Node base class:  Node.FS.File for files, Node.Alias for aliases,
etc.  Dependency information is kept here in the base class, and
information specific to files/aliases/etc. is in the subclass.  The
goal, if we've done this correctly, is that any type of "thing" should
be able to depend on any other type of "thing."

"""

#
# Copyright (c) 2001, 2002, 2003, 2004 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "/home/scons/scons/branch.0/branch.96/baseline/src/engine/SCons/Node/__init__.py 0.96.90.D001 2005/02/15 20:11:37 knight"



import copy
import string
import UserList

from SCons.Debug import logInstanceCreation
import SCons.Executor
import SCons.SConsign
import SCons.Util

# Node states
#
# These are in "priority" order, so that the maximum value for any
# child/dependency of a node represents the state of that node if
# it has no builder of its own.  The canonical example is a file
# system directory, which is only up to date if all of its children
# were up to date.
pending = 1
executing = 2
up_to_date = 3
executed = 4
failed = 5
stack = 6 # nodes that are in the current Taskmaster execution stack

# controls whether implicit dependencies are cached:
implicit_cache = 0

# controls whether implicit dep changes are ignored:
implicit_deps_unchanged = 0

# controls whether the cached implicit deps are ignored:
implicit_deps_changed = 0

# A variable that can be set to an interface-specific function be called
# to annotate a Node with information about its creation.
def do_nothing(node): pass

Annotate = do_nothing

class BuildInfo:
    def __cmp__(self, other):
        return cmp(self.__dict__, other.__dict__)

class Node:
    """The base Node class, for entities that we know how to
    build, or use to build other Nodes.
    """

    __metaclass__ = SCons.Memoize.Memoized_Metaclass

    class Attrs:
        pass

    def __init__(self):
        if __debug__: logInstanceCreation(self, 'Node')
        # Note that we no longer explicitly initialize a self.builder
        # attribute to None here.  That's because the self.builder
        # attribute may be created on-the-fly later by a subclass (the
        # canonical example being a builder to fetch a file from a
        # source code system like CVS or Subversion).

        # Each list of children that we maintain is accompanied by a
        # dictionary used to look up quickly whether a node is already
        # present in the list.  Empirical tests showed that it was
        # fastest to maintain them as side-by-side Node attributes in
        # this way, instead of wrapping up each list+dictionary pair in
        # a class.  (Of course, we could always still do that in the
        # future if we had a good reason to...).
        self.sources = []       # source files used to build node
        self.sources_dict = {}
        self.depends = []       # explicit dependencies (from Depends)
        self.depends_dict = {}
        self.ignore = []        # dependencies to ignore
        self.ignore_dict = {}
        self.implicit = None    # implicit (scanned) dependencies (None means not scanned yet)
        self.waiting_parents = []
        self.wkids = None       # Kids yet to walk, when it's an array

        self.env = None
        self.state = None
        self.precious = None
        self.always_build = None
        self.found_includes = {}
        self.includes = None
        self.attributes = self.Attrs() # Generic place to stick information about the Node.
        self.side_effect = 0 # true iff this node is a side effect
        self.side_effects = [] # the side effects of building this target
        self.pre_actions = []
        self.post_actions = []
        self.linked = 0 # is this node linked to the build directory? 

        # Let the interface in which the build engine is embedded
        # annotate this Node with its own info (like a description of
        # what line in what file created the node, for example).
        Annotate(self)

    def get_suffix(self):
        return ''

    def get_build_env(self):
        """Fetch the appropriate Environment to build this node.
        __cacheable__"""
        return self.get_executor().get_build_env()

    def get_build_scanner_path(self, scanner):
        """Fetch the appropriate scanner path for this node."""
        return self.get_executor().get_build_scanner_path(scanner)

    def set_executor(self, executor):
        """Set the action executor for this node."""
        self.executor = executor

    def get_executor(self, create=1):
        """Fetch the action executor for this node.  Create one if
        there isn't already one, and requested to do so."""
        try:
            executor = self.executor
        except AttributeError:
            if not create:
                raise
            try:
                act = self.builder.action
            except AttributeError:
                executor = SCons.Executor.Null()
            else:
                if self.pre_actions:
                    act = self.pre_actions + act
                if self.post_actions:
                    act = act + self.post_actions
                executor = SCons.Executor.Executor(act,
                                                   self.env or self.builder.env,
                                                   [self.builder.overrides],
                                                   [self],
                                                   self.sources)
            self.executor = executor
        return executor

    def executor_cleanup(self):
        """Let the executor clean up any cached information."""
        try:
            executor = self.get_executor(create=None)
        except AttributeError:
            pass
        else:
            executor.cleanup()

    def reset_executor(self):
        "Remove cached executor; forces recompute when needed."
        try:
            delattr(self, 'executor')
        except AttributeError:
            pass

    def retrieve_from_cache(self):
        """Try to retrieve the node's content from a cache

        This method is called from multiple threads in a parallel build,
        so only do thread safe stuff here. Do thread unsafe stuff in
        built().

        Returns true iff the node was successfully retrieved.
        """
        return 0
        
    def build(self, **kw):
        """Actually build the node.

        This method is called from multiple threads in a parallel build,
        so only do thread safe stuff here. Do thread unsafe stuff in
        built().
        """
        def errfunc(stat, node=self):
            raise SCons.Errors.BuildError(node=node, errstr="Error %d" % stat)
        executor = self.get_executor()
        apply(executor, (self, errfunc), kw)

    def built(self):
        """Called just after this node is successfully built."""

        # Clear the implicit dependency caches of any Nodes
        # waiting for this Node to be built.
        for parent in self.waiting_parents:
            parent.implicit = None
            parent.del_binfo()
        
        try:
            new_binfo = self.binfo
        except AttributeError:
            # Node arrived here without build info; apparently it
            # doesn't need it, so don't bother calculating or storing
            # it.
            new_binfo = None

        # Reset this Node's cached state since it was just built and
        # various state has changed.
        self.clear()

        # Had build info, so it should be stored in the signature
        # cache.  However, if the build info included a content
        # signature then it should be recalculated before being
        # stored.
        
        if new_binfo:
            if hasattr(new_binfo, 'csig'):
                new_binfo = self.gen_binfo()  # sets self.binfo
            else:
                self.binfo = new_binfo
            self.store_info(new_binfo)

    def add_to_waiting_parents(self, node):
        self.waiting_parents.append(node)

    def call_for_all_waiting_parents(self, func):
        func(self)
        for parent in self.waiting_parents:
            parent.call_for_all_waiting_parents(func)

    def postprocess(self):
        """Clean up anything we don't need to hang onto after we've
        been built."""
        self.executor_cleanup()

    def clear(self):
        """Completely clear a Node of all its cached state (so that it
        can be re-evaluated by interfaces that do continuous integration
        builds).
        __reset_cache__
        """
        self.executor_cleanup()
        self.del_binfo()
        self.del_cinfo()
        try:
            delattr(self, '_calculated_sig')
        except AttributeError:
            pass
        self.includes = None
        self.found_includes = {}
        self.implicit = None

        self.waiting_parents = []

    def visited(self):
        """Called just after this node has been visited
        without requiring a build.."""
        pass

    def depends_on(self, nodes):
        """Does this node depend on any of 'nodes'? __cacheable__"""
        return reduce(lambda D,N,C=self.children(): D or (N in C), nodes, 0)

    def builder_set(self, builder):
        "__cache_reset__"
        self.builder = builder

    def has_builder(self):
        """Return whether this Node has a builder or not.

        In Boolean tests, this turns out to be a *lot* more efficient
        than simply examining the builder attribute directly ("if
        node.builder: ..."). When the builder attribute is examined
        directly, it ends up calling __getattr__ for both the __len__
        and __nonzero__ attributes on instances of our Builder Proxy
        class(es), generating a bazillion extra calls and slowing
        things down immensely.
        """
        try:
            b = self.builder
        except AttributeError:
            # There was no explicit builder for this Node, so initialize
            # the self.builder attribute to None now.
            self.builder = None
            b = self.builder
        return not b is None

    def set_explicit(self, is_explicit):
        self.is_explicit = is_explicit

    def has_explicit_builder(self):
        """Return whether this Node has an explicit builder

        This allows an internal Builder created by SCons to be marked
        non-explicit, so that it can be overridden by an explicit
        builder that the user supplies (the canonical example being
        directories)."""
        try:
            return self.is_explicit
        except AttributeError:
            self.is_explicit = None
            return self.is_explicit

    def get_builder(self, default_builder=None):
        """Return the set builder, or a specified default value"""
        try:
            return self.builder
        except AttributeError:
            return default_builder

    multiple_side_effect_has_builder = has_builder

    def is_derived(self):
        """
        Returns true iff this node is derived (i.e. built).

        This should return true only for nodes whose path should be in
        the build directory when duplicate=0 and should contribute their build
        signatures when they are used as source files to other derived files. For
        example: source with source builders are not derived in this sense,
        and hence should not return true.
        __cacheable__
        """
        return self.has_builder() or self.side_effect

    def is_pseudo_derived(self):
        """
        Returns true iff this node is built, but should use a source path
        when duplicate=0 and should contribute a content signature (i.e.
        source signature) when used as a source for other derived files.
        """
        return 0

    def alter_targets(self):
        """Return a list of alternate targets for this Node.
        """
        return [], None

    def get_found_includes(self, env, scanner, path):
        """Return the scanned include lines (implicit dependencies)
        found in this node.

        The default is no implicit dependencies.  We expect this method
        to be overridden by any subclass that can be scanned for
        implicit dependencies.
        """
        return []

    def get_implicit_deps(self, env, scanner, path):
        """Return a list of implicit dependencies for this node.

        This method exists to handle recursive invocation of the scanner
        on the implicit dependencies returned by the scanner, if the
        scanner's recursive flag says that we should.
        """
        if not scanner:
            return []

        # Give the scanner a chance to select a more specific scanner
        # for this Node.
        scanner = scanner.select(self)

        nodes = [self]
        seen = {}
        seen[self] = 1
        deps = []
        while nodes:
            n = nodes.pop(0)
            d = filter(lambda x, seen=seen: not seen.has_key(x),
                       n.get_found_includes(env, scanner, path))
            if d:
                deps.extend(d)
                for n in d:
                    seen[n] = 1
                nodes.extend(scanner.recurse_nodes(d))

        return deps

    def implicit_factory(self, path):
        """
        Turn a cache implicit dependency path into a node.
        This is called so many times that doing caching
        here is a significant performance boost.
        __cacheable__
        """
        return self.builder.source_factory(path)

    def get_scanner(self, env, kw={}):
        return env.get_scanner(self.scanner_key())

    def get_source_scanner(self, node):
        """Fetch the source scanner for the specified node

        NOTE:  "self" is the target being built, "node" is
        the source file for which we want to fetch the scanner.

        Implies self.has_builder() is true; again, expect to only be
        called from locations where this is already verified.

        This function may be called very often; it attempts to cache
        the scanner found to improve performance.
        """
        scanner = None
        try:
            scanner = self.builder.source_scanner
        except AttributeError:
            pass
        if not scanner:
            # The builder didn't have an explicit scanner, so go look up
            # a scanner from env['SCANNERS'] based on the node's scanner
            # key (usually the file extension).
            scanner = self.get_scanner(self.get_build_env())
        if scanner:
            scanner = scanner.select(node)
        return scanner

    def add_to_implicit(self, deps):
        if not hasattr(self, 'implicit') or self.implicit is None:
            self.implicit = []
            self.implicit_dict = {}
            self._children_reset()
        self._add_child(self.implicit, self.implicit_dict, deps)

    def scan(self):
        """Scan this node's dependents for implicit dependencies."""
        # Don't bother scanning non-derived files, because we don't
        # care what their dependencies are.
        # Don't scan again, if we already have scanned.
        if not self.implicit is None:
            return
        self.implicit = []
        self.implicit_dict = {}
        self._children_reset()
        if not self.has_builder():
            return

        build_env = self.get_build_env()

        # Here's where we implement --implicit-cache.
        if implicit_cache and not implicit_deps_changed:
            implicit = self.get_stored_implicit()
            if implicit is not None:
                implicit = map(self.implicit_factory, implicit)
                self._add_child(self.implicit, self.implicit_dict, implicit)
                calc = build_env.get_calculator()
                if implicit_deps_unchanged or self.current(calc):
                    return
                else:
                    # one of this node's sources has changed, so
                    # we need to recalculate the implicit deps,
                    # and the bsig:
                    self.implicit = []
                    self.implicit_dict = {}
                    self._children_reset()
                    self.del_binfo()

        scanner = self.builder.source_scanner
        self.get_executor().scan(scanner)

        # scan this node itself for implicit dependencies
        scanner = self.builder.target_scanner
        if scanner:
            path = self.get_build_scanner_path(scanner)
            deps = self.get_implicit_deps(build_env, scanner, path)
            self._add_child(self.implicit, self.implicit_dict, deps)

        # XXX See note above re: --implicit-cache.
        #if implicit_cache:
        #    self.store_implicit()

    def scanner_key(self):
        return None

    def env_set(self, env, safe=0):
        if safe and self.env:
            return
        self.env = env

    def calculator(self):
        import SCons.Defaults
        
        env = self.env or SCons.Defaults.DefaultEnvironment()
        return env.get_calculator()

    def calc_signature(self, calc=None):
        """
        Select and calculate the appropriate build signature for a node.
        __cacheable__

        self - the node
        calc - the signature calculation module
        returns - the signature
        """
        if self.is_derived():
            import SCons.Defaults

            env = self.env or SCons.Defaults.DefaultEnvironment()
            if env.use_build_signature():
                return self.calc_bsig(calc)
        elif not self.rexists():
            return None
        return self.calc_csig(calc)

    def new_binfo(self):
        return BuildInfo()

    def del_binfo(self):
        """Delete the bsig from this node."""
        try:
            delattr(self, 'binfo')
        except AttributeError:
            pass

    def calc_bsig(self, calc=None):
        try:
            return self.binfo.bsig
        except AttributeError:
            self.binfo = self.gen_binfo(calc)
            return self.binfo.bsig

    def gen_binfo(self, calc=None, scan=1):
        """
        Generate a node's build signature, the digested signatures
        of its dependency files and build information.

        node - the node whose sources will be collected
        cache - alternate node to use for the signature cache
        returns - the build signature

        This no longer handles the recursive descent of the
        node's children's signatures.  We expect that they're
        already built and updated by someone else, if that's
        what's wanted.
        __cacheable__
        """

        if calc is None:
            calc = self.calculator()

        binfo = self.new_binfo()

        if scan:
            self.scan()

        executor = self.get_executor()

        sourcelist = executor.get_source_binfo(calc)
        depends = self.depends
        implicit = self.implicit or []

        if self.ignore:
            sourcelist = filter(lambda t, s=self: s.do_not_ignore(t[0]), sourcelist)
            depends = filter(self.do_not_ignore, depends)
            implicit = filter(self.do_not_ignore, implicit)

        def calc_signature(node, calc=calc):
            return node.calc_signature(calc)
        sourcesigs = map(lambda t: t[1], sourcelist)
        dependsigs = map(calc_signature, depends)
        implicitsigs = map(calc_signature, implicit)

        sigs = sourcesigs + dependsigs + implicitsigs

        if self.has_builder():
            binfo.bact = str(executor)
            binfo.bactsig = calc.module.signature(executor)
            sigs.append(binfo.bactsig)

        binfo.bsources = map(lambda t: t[2], sourcelist)
        binfo.bdepends = map(str, depends)
        binfo.bimplicit = map(str, implicit)

        binfo.bsourcesigs = sourcesigs
        binfo.bdependsigs = dependsigs
        binfo.bimplicitsigs = implicitsigs

        binfo.bsig = calc.module.collect(filter(None, sigs))

        return binfo

    def del_cinfo(self):
        try:
            del self.binfo.csig
        except AttributeError:
            pass

    def calc_csig(self, calc=None):
        try:
            binfo = self.binfo
        except AttributeError:
            binfo = self.binfo = self.new_binfo()
        try:
            return binfo.csig
        except AttributeError:
            if calc is None:
                calc = self.calculator()
            binfo.csig = calc.module.signature(self)
            self.store_info(binfo)
            return binfo.csig

    def store_info(self, obj):
        """Make the build signature permanent (that is, store it in the
        .sconsign file or equivalent)."""
        pass

    def get_stored_info(self):
        return None

    def get_stored_implicit(self):
        """Fetch the stored implicit dependencies"""
        return None

    def set_precious(self, precious = 1):
        """Set the Node's precious value."""
        self.precious = precious

    def set_always_build(self, always_build = 1):
        """Set the Node's always_build value."""
        self.always_build = always_build

    def exists(self):
        """Does this node exists?"""
        # All node exist by default:
        return 1
    
    def rexists(self):
        """Does this node exist locally or in a repositiory?"""
        # There are no repositories by default:
        return self.exists()

    def missing(self):
        """__cacheable__"""
        return not self.is_derived() and \
               not self.is_pseudo_derived() and \
               not self.linked and \
               not self.rexists()
    
    def prepare(self):
        """Prepare for this Node to be created.
        The default implemenation checks that all children either exist
        or are derived.
        """
        l = self.depends
        if not self.implicit is None:
            l = l + self.implicit
        missing_sources = self.get_executor().get_missing_sources() \
                          + filter(lambda c: c.missing(), l)
        if missing_sources:
            desc = "Source `%s' not found, needed by target `%s'." % (missing_sources[0], self)
            raise SCons.Errors.StopError, desc

    def remove(self):
        """Remove this Node:  no-op by default."""
        return None

    def add_dependency(self, depend):
        """Adds dependencies."""
        try:
            self._add_child(self.depends, self.depends_dict, depend)
        except TypeError, e:
            e = e.args[0]
            if SCons.Util.is_List(e):
                s = map(str, e)
            else:
                s = str(e)
            raise SCons.Errors.UserError("attempted to add a non-Node dependency to %s:\n\t%s is a %s, not a Node" % (str(self), s, type(e)))

    def add_ignore(self, depend):
        """Adds dependencies to ignore."""
        try:
            self._add_child(self.ignore, self.ignore_dict, depend)
        except TypeError, e:
            e = e.args[0]
            if SCons.Util.is_List(e):
                s = map(str, e)
            else:
                s = str(e)
            raise SCons.Errors.UserError("attempted to ignore a non-Node dependency of %s:\n\t%s is a %s, not a Node" % (str(self), s, type(e)))

    def add_source(self, source):
        """Adds sources."""
        try:
            self._add_child(self.sources, self.sources_dict, source)
        except TypeError, e:
            e = e.args[0]
            if SCons.Util.is_List(e):
                s = map(str, e)
            else:
                s = str(e)
            raise SCons.Errors.UserError("attempted to add a non-Node as source of %s:\n\t%s is a %s, not a Node" % (str(self), s, type(e)))

    def _add_child(self, collection, dict, child):
        """Adds 'child' to 'collection', first checking 'dict' to see
        if it's already present."""
        if type(child) is not type([]):
            child = [child]
        for c in child:
            if not isinstance(c, Node):
                raise TypeError, c
        added = None
        for c in child:
            if not dict.has_key(c):
                collection.append(c)
                dict[c] = 1
                added = 1
        if added:
            self._children_reset()

    def add_wkid(self, wkid):
        """Add a node to the list of kids waiting to be evaluated"""
        if self.wkids != None:
            self.wkids.append(wkid)

    def _children_reset(self):
        "__cache_reset__"
        # We need to let the Executor clear out any calculated
        # bsig info that it's cached so we can re-calculate it.
        self.executor_cleanup()

    def do_not_ignore(self, node):
        return node not in self.ignore

    def _all_children_get(self):
        # The return list may contain duplicate Nodes, especially in
        # source trees where there are a lot of repeated #includes
        # of a tangle of .h files.  Profiling shows, however, that
        # eliminating the duplicates with a brute-force approach that
        # preserves the order (that is, something like:
        #
        #       u = []
        #       for n in list:
        #           if n not in u:
        #               u.append(n)"
        #
        # takes more cycles than just letting the underlying methods
        # hand back cached values if a Node's information is requested
        # multiple times.  (Other methods of removing duplicates, like
        # using dictionary keys, lose the order, and the only ordered
        # dictionary patterns I found all ended up using "not in"
        # internally anyway...)
        if self.implicit is None:
            return self.sources + self.depends
        else:
            return self.sources + self.depends + self.implicit

    def _children_get(self):
        "__cacheable__"
        children = self._all_children_get()
        if self.ignore:
            children = filter(self.do_not_ignore, children)
        return children

    def all_children(self, scan=1):
        """Return a list of all the node's direct children."""
        if scan:
            self.scan()
        return self._all_children_get()

    def children(self, scan=1):
        """Return a list of the node's direct children, minus those
        that are ignored by this node."""
        if scan:
            self.scan()
        return self._children_get()

    def set_state(self, state):
        self.state = state

    def get_state(self):
        return self.state

    def current(self, calc=None):
        """Default check for whether the Node is current: unknown Node
        subtypes are always out of date, so they will always get built."""
        return None

    def children_are_up_to_date(self, calc=None):
        """Alternate check for whether the Node is current:  If all of
        our children were up-to-date, then this Node was up-to-date, too.

        The SCons.Node.Alias and SCons.Node.Python.Value subclasses
        rebind their current() method to this method."""
        # Allow the children to calculate their signatures.
        self.binfo = self.gen_binfo(calc)
        if self.always_build:
            return None
        state = 0
        for kid in self.children(None):
            s = kid.get_state()
            if s and (not state or s > state):
                state = s
        return (state == 0 or state == SCons.Node.up_to_date)

    def is_literal(self):
        """Always pass the string representation of a Node to
        the command interpreter literally."""
        return 1

    def add_pre_action(self, act):
        """Adds an Action performed on this Node only before
        building it."""
        self.pre_actions.append(act)
        # executor must be recomputed to include new pre-actions
        self.reset_executor()

    def add_post_action(self, act):
        """Adds and Action performed on this Node only after
        building it."""
        self.post_actions.append(act)
        # executor must be recomputed to include new pre-actions
        self.reset_executor()

    def render_include_tree(self):
        """
        Return a text representation, suitable for displaying to the
        user, of the include tree for the sources of this node.
        """
        if self.is_derived() and self.env:
            env = self.get_build_env()
            for s in self.sources:
                scanner = self.get_source_scanner(s)
                path = self.get_build_scanner_path(scanner)
                def f(node, env=env, scanner=scanner, path=path):
                    return node.get_found_includes(env, scanner, path)
                return SCons.Util.render_tree(s, f, 1)
        else:
            return None

    def get_abspath(self):
        """
        Return an absolute path to the Node.  This will return simply
        str(Node) by default, but for Node types that have a concept of
        relative path, this might return something different.
        """
        return str(self)

    def for_signature(self):
        """
        Return a string representation of the Node that will always
        be the same for this particular Node, no matter what.  This
        is by contrast to the __str__() method, which might, for
        instance, return a relative path for a file Node.  The purpose
        of this method is to generate a value to be used in signature
        calculation for the command line used to build a target, and
        we use this method instead of str() to avoid unnecessary
        rebuilds.  This method does not need to return something that
        would actually work in a command line; it can return any kind of
        nonsense, so long as it does not change.
        """
        return str(self)

    def get_string(self, for_signature):
        """This is a convenience function designed primarily to be
        used in command generators (i.e., CommandGeneratorActions or
        Environment variables that are callable), which are called
        with a for_signature argument that is nonzero if the command
        generator is being called to generate a signature for the
        command line, which determines if we should rebuild or not.

        Such command generators should use this method in preference
        to str(Node) when converting a Node to a string, passing
        in the for_signature parameter, such that we will call
        Node.for_signature() or str(Node) properly, depending on whether
        we are calculating a signature or actually constructing a
        command line."""
        if for_signature:
            return self.for_signature()
        return str(self)

    def get_subst_proxy(self):
        """
        This method is expected to return an object that will function
        exactly like this Node, except that it implements any additional
        special features that we would like to be in effect for
        Environment variable substitution.  The principle use is that
        some Nodes would like to implement a __getattr__() method,
        but putting that in the Node type itself has a tendency to kill
        performance.  We instead put it in a proxy and return it from
        this method.  It is legal for this method to return self
        if no new functionality is needed for Environment substitution.
        """
        return self

    def explain(self):
        if not self.exists():
            return "building `%s' because it doesn't exist\n" % self

        old = self.get_stored_info()
        if old is None:
            return None

        def dictify(result, kids, sigs):
            for k, s in zip(kids, sigs):
                result[k] = s

        try:
            old_bkids = old.bsources + old.bdepends + old.bimplicit
        except AttributeError:
            return "Cannot explain why `%s' is being rebuilt: No previous build information found\n" % self

        osig = {}
        dictify(osig, old.bsources, old.bsourcesigs)
        dictify(osig, old.bdepends, old.bdependsigs)
        dictify(osig, old.bimplicit, old.bimplicitsigs)

        new_bsources = map(str, self.binfo.bsources)
        new_bdepends = map(str, self.binfo.bdepends)
        new_bimplicit = map(str, self.binfo.bimplicit)

        nsig = {}
        dictify(nsig, new_bsources, self.binfo.bsourcesigs)
        dictify(nsig, new_bdepends, self.binfo.bdependsigs)
        dictify(nsig, new_bimplicit, self.binfo.bimplicitsigs)

        new_bkids = new_bsources + new_bdepends + new_bimplicit
        lines = map(lambda x: "`%s' is no longer a dependency\n" % x,
                    filter(lambda x, nk=new_bkids: not x in nk, old_bkids))

        for k in new_bkids:
            if not k in old_bkids:
                lines.append("`%s' is a new dependency\n" % k)
            elif osig[k] != nsig[k]:
                lines.append("`%s' changed\n" % k)

        if len(lines) == 0 and old_bkids != new_bkids:
            lines.append("the dependency order changed:\n" +
                         "%sold: %s\n" % (' '*15, old_bkids) +
                         "%snew: %s\n" % (' '*15, new_bkids))

        if len(lines) == 0:
            newact, newactsig = self.binfo.bact, self.binfo.bactsig
            def fmt_with_title(title, strlines):
                lines = string.split(strlines, '\n')
                sep = '\n' + ' '*(15 + len(title))
                return ' '*15 + title + string.join(lines, sep) + '\n'
            if old.bactsig != newactsig:
                if old.bact == newact:
                    lines.append("the contents of the build action changed\n" +
                                 fmt_with_title('action: ', newact))
                else:
                    lines.append("the build action changed:\n" +
                                 fmt_with_title('old: ', old.bact) +
                                 fmt_with_title('new: ', newact))

        if len(lines) == 0:
            return "rebuilding `%s' for unknown reasons\n" % self

        preamble = "rebuilding `%s' because" % self
        if len(lines) == 1:
            return "%s %s"  % (preamble, lines[0])
        else:
            lines = ["%s:\n" % preamble] + lines
            return string.join(lines, ' '*11)

l = [1]
ul = UserList.UserList([2])
try:
    l.extend(ul)
except TypeError:
    def NodeList(l):
        return l
else:
    class NodeList(UserList.UserList):
        def __str__(self):
            return str(map(str, self.data))
del l
del ul

if not SCons.Memoize.has_metaclass:
    _Base = Node
    class Node(SCons.Memoize.Memoizer, _Base):
        def __init__(self, *args, **kw):
            apply(_Base.__init__, (self,)+args, kw)
            SCons.Memoize.Memoizer.__init__(self)


def get_children(node, parent): return node.children()
def ignore_cycle(node, stack): pass
def do_nothing(node, parent): pass

class Walker:
    """An iterator for walking a Node tree.

    This is depth-first, children are visited before the parent.
    The Walker object can be initialized with any node, and
    returns the next node on the descent with each next() call.
    'kids_func' is an optional function that will be called to
    get the children of a node instead of calling 'children'.
    'cycle_func' is an optional function that will be called
    when a cycle is detected.

    This class does not get caught in node cycles caused, for example,
    by C header file include loops.
    """
    def __init__(self, node, kids_func=get_children,
                             cycle_func=ignore_cycle,
                             eval_func=do_nothing):
        self.kids_func = kids_func
        self.cycle_func = cycle_func
        self.eval_func = eval_func
        node.wkids = copy.copy(kids_func(node, None))
        self.stack = [node]
        self.history = {} # used to efficiently detect and avoid cycles
        self.history[node] = None

    def next(self):
        """Return the next node for this walk of the tree.

        This function is intentionally iterative, not recursive,
        to sidestep any issues of stack size limitations.
        """

        while self.stack:
            if self.stack[-1].wkids:
                node = self.stack[-1].wkids.pop(0)
                if not self.stack[-1].wkids:
                    self.stack[-1].wkids = None
                if self.history.has_key(node):
                    self.cycle_func(node, self.stack)
                else:
                    node.wkids = copy.copy(self.kids_func(node, self.stack[-1]))
                    self.stack.append(node)
                    self.history[node] = None
            else:
                node = self.stack.pop()
                del self.history[node]
                if node:
                    if self.stack:
                        parent = self.stack[-1]
                    else:
                        parent = None
                    self.eval_func(node, parent)
                return node
        return None

    def is_done(self):
        return not self.stack


arg2nodes_lookups = []
