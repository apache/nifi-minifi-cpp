package org.apache.nifi.processor;

import org.apache.nifi.annotation.behavior.DynamicProperty;
import org.apache.nifi.annotation.behavior.DynamicRelationship;
import org.apache.nifi.annotation.documentation.CapabilityDescription;
import org.apache.nifi.annotation.lifecycle.OnDisabled;
import org.apache.nifi.annotation.lifecycle.OnEnabled;
import org.apache.nifi.annotation.lifecycle.OnScheduled;
import org.apache.nifi.bundle.Bundle;
import org.apache.nifi.bundle.BundleDetails;
import org.apache.nifi.components.ConfigurableComponent;
import org.apache.nifi.components.PropertyDescriptor;
import org.apache.nifi.controller.ControllerService;
import org.apache.nifi.init.ConfigurableComponentInitializer;
import org.apache.nifi.init.ConfigurableComponentInitializerFactory;
import org.apache.nifi.nar.*;
import org.apache.nifi.reporting.InitializationException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.lang.annotation.Annotation;
import java.lang.reflect.Method;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.stream.Collectors;

/**
 * Basic class loader that references the dependencies within NARs
 */
public class JniClassLoader  {

    private static final Logger logger = LoggerFactory
            .getLogger(JniClassLoader.class);


    /**
     * Pointer to native object.
     */
    private long nativePtr;


    /**
     * Parent class loader reference ( will ultimately be a reference to the JNI class loader).
     */
    private ClassLoader parent= null;

    private static ConcurrentHashMap<String,Class<?>> classes = new ConcurrentHashMap<>();

    private ConcurrentHashMap<Map.Entry<String,String>,Method> onScheduledMethod = new ConcurrentHashMap<>();

    private ConcurrentHashMap<String,JniComponent> componentMap = new ConcurrentHashMap<>();

    private List<JniBundle> bundles = new ArrayList<>();

    private ConcurrentHashMap<String, BundleDetails> bundleMap = new ConcurrentHashMap<>();
    private ConcurrentHashMap<String, File> fileMap = new ConcurrentHashMap<>();
    private ConcurrentHashMap<String, NarClassLoader> loaderMap = new ConcurrentHashMap<>();

    public JniClassLoader(){
    }


    static class Dependency
    {
        String artifactName;
        AtomicBoolean visited;
        List<Dependency> subDependencies;

        Dependency(final String name )
        {
            artifactName = name;
            visited = new AtomicBoolean(false);
            subDependencies =new ArrayList<>();

        }
        public void addDependency(Dependency neighbourDependency)
        {
            this.subDependencies.add(neighbourDependency);
        }
        public List<Dependency> getSubDependencies() {
            return subDependencies;
        }
        public String toString()
        {
            return artifactName;
        }

        @Override
        public int hashCode(){
            return artifactName.hashCode();
        }

        @Override
        public boolean equals(Object obj){
            return artifactName.equals(obj);
        }
    }

    public  void sortDependencies(final Stack<Dependency> dependencyStack, Dependency dependency)
    {
        if (dependency != null) {

            dependency.getSubDependencies().stream().filter(x -> !x.visited.get()).forEach(
                    x -> {
                        sortDependencies(dependencyStack, x);
                        x.visited.set(true);
                    }
            );
        }
        dependencyStack.push(dependency);
    }

    /**
     * Initializes the nar directory. This is the mainstay of the JNI class loader's initialization.
     * @param narDirectory directory in which we place our nars
     * @param narWriteBase the directory to which we will write or explode our NARs
     * @param docsDir nar document directory
     * @param parent parent class loader, if there is one
     * @throws IOException
     * @throws ClassNotFoundException
     */
    public synchronized void initializeNarDirectory(final String narDirectory,final String narWriteBase,final String docsDir, ClassLoader parent) throws IOException, ClassNotFoundException{
        // unpack the nar
        if (this.parent != null)
            throw new IllegalArgumentException("Already initialized");

        this.parent = parent;
        List<File> paths = new ArrayList<>();
        File narDeploy = new File(narWriteBase);

        JniUnpacker.unpackNars(new File(narDirectory),narDeploy,paths);

        List<File> directories = Arrays.asList(narDeploy.listFiles(File::isDirectory));

        for(File narPath : directories) {
            final BundleDetails details = NarBundleUtil.fromNarDirectory(narPath);
            bundleMap.put(details.getCoordinate().getId(), details );
            fileMap.put(details.getCoordinate().getId(), narPath);
        }



        // these two allow us to see load those which do not have dependencies followed by those that do
        // if a dependency exists within this set then we can chain those class loaders as needed.
        final Map<String, Dependency> dependencyMap = new HashMap<>();
        final List<String> nonDeps = new ArrayList<>();
        bundleMap.entrySet().stream().filter((e) ->{
            return e.getValue().getDependencyCoordinate() == null;
        }).collect(Collectors.toList()).forEach( entry ->{
            File path = fileMap.get(entry.getKey());
            if (path != null) {
                try {
                    nonDeps.add(entry.getKey());
                    final NarClassLoader loader = new NarClassLoader(path, parent);

                    loaderMap.put(entry.getKey(),loader);

                    Dependency dep = new Dependency(entry.getKey());
                    dependencyMap.put(entry.getKey(), dep);

                   discoverAndLoad(new Bundle(entry.getValue(), loader));

                } catch (ClassNotFoundException e) {
                    logger.error("Could not create NarClassLoader",e);
                } catch (IOException e) {
                    logger.error("Could not create NarClassLoader",e);
                }

                Dependency dep = new Dependency(entry.getKey());
                dependencyMap.put(entry.getKey(), dep);
            }
        });


        bundleMap.entrySet().stream().filter((e) ->{
            return e.getValue().getDependencyCoordinate() != null;
        }).collect(Collectors.toList()).forEach(entry ->{
            File path = fileMap.get(entry.getKey());
            if (path != null) {

                Dependency dep = dependencyMap.get(entry.getValue().getCoordinate().getId());
                if (dep == null) {
                    dep = new Dependency(entry.getValue().getCoordinate().getId());
                    dependencyMap.put(entry.getValue().getCoordinate().getId(), dep);
                }
                Dependency subDep = dependencyMap.get(entry.getValue().getDependencyCoordinate().getId());
                if (subDep == null) {
                    subDep = new Dependency(entry.getValue().getDependencyCoordinate().getId());
                    dependencyMap.put(entry.getValue().getDependencyCoordinate().getId(), subDep);
                }
                dep.addDependency(subDep);
            }

        });

        bundleMap.entrySet().stream().filter((e) ->{
            return e.getValue().getDependencyCoordinate() != null;
        }).collect(Collectors.toList()).forEach(entry ->{

                try {

                    Dependency dep = dependencyMap.get(entry.getKey());

                    Stack<Dependency> stackedDependencies = new Stack<>();

                    sortDependencies(stackedDependencies,dep);

                    NarClassLoader loader = null;
                    NarClassLoader depLoader = null;
                    // test the dependencies to ensure we've loaded them
                    // and performed a topological sort.
                    for(Dependency sortedDependency : stackedDependencies) {
                        depLoader = loaderMap.get(sortedDependency);
                        if (depLoader == null) {
                        File path = fileMap.get(sortedDependency.artifactName);
                        BundleDetails deets = bundleMap.get(sortedDependency.artifactName);
                        if (deets.getDependencyCoordinate() == null){
                            loader = null;
                        }
                        else
                        loader = loaderMap.get(deets.getDependencyCoordinate().getId());
                            depLoader = new NarClassLoader(path, loader == null ? parent : loader);
                            loaderMap.put(sortedDependency.artifactName, depLoader);
                        }
                    }

                    NarClassLoader thisLoaader = loaderMap.get(entry.getKey());
                    discoverAndLoad(new Bundle(entry.getValue(), thisLoaader));

                } catch (ClassNotFoundException e) {
                    logger.error("Could not create NarClassLoader",e);
                } catch (Throwable e) {
                    logger.error("Could not create NarClassLoader",e);
                }

        });
    }

    protected void discoverAndLoad(Bundle bundle){
        List<JniComponent> components = discoverExtensions(bundle);

        componentMap.putAll(
                components.stream().collect(Collectors.toMap(JniComponent::getType, jniComponent -> jniComponent)));


        bundles.add(new JniBundle(bundle.getBundleDetails(), components));

    }

    public Class getClass(final String className) throws ClassNotFoundException {
        Class clazz = classes.get(className);
        if (clazz == null){
            clazz = parent.loadClass(className);
        }
        return clazz;
    }

    public List<JniBundle> getBundles(){
        return Collections.unmodifiableList(bundles);
    }

    /**
     * Loads all FlowFileProcessor, FlowFileComparator, ReportingTask class types that can be found on the bootstrap classloader and by creating classloaders for all NARs found within the classpath.
     * @param bundle the bundles to scan through in search of extensions
     */
    public static List<JniComponent> discoverExtensions(final Bundle bundle) {
        // get the current context class loader
        ClassLoader currentContextClassLoader = Thread.currentThread().getContextClassLoader();

        List<JniComponent> components = new ArrayList<>();

            // Must set the context class loader to the nar classloader itself
            // so that static initialization techniques that depend on the context class loader will work properly
            final ClassLoader ncl = bundle.getClassLoader();
            Thread.currentThread().setContextClassLoader(ncl);
            components.addAll( loadProcessors(bundle) );


        // restore the current context class loader if appropriate
        if (currentContextClassLoader != null) {
            Thread.currentThread().setContextClassLoader(currentContextClassLoader);
        }
        return components;
    }

    /**
     * Loads extensions from the specified bundle.
     *
     * @param bundle from which to load extensions
     */
    @SuppressWarnings("unchecked")
    private static List<JniComponent> loadProcessors(final Bundle bundle) {
        List<JniComponent> components = new ArrayList<>();
        ServiceLoader<?> serviceLoader = ServiceLoader.load(Processor.class, bundle.getClassLoader());
        Iterator<?> sli = serviceLoader.iterator();
            while(sli.hasNext()){
                try {
                Object o = sli.next();
                // create a cache of temp ConfigurableComponent instances, the initialize here has to happen before the checks below
                if (o instanceof ConfigurableComponent) {

                        final ConfigurableComponent configurableComponent = (ConfigurableComponent) o;
                        initializeTempComponent(configurableComponent);
                        if (configurableComponent instanceof Processor ) {
                            final Processor processor = Processor.class.cast(configurableComponent);
                            if (processor != null) {
                                List<PropertyDescriptor> descriptors = processor.getPropertyDescriptors();
                                final String description = getDescription(processor.getClass());
                                classes.put(processor.getClass().getCanonicalName(),processor.getClass());
                                final DynamicProperty dynProperty = getDynamicPropertyAnnotation(processor.getClass());
                                final DynamicRelationship dynRelationShip = getDynamicRelationshipAnnotation(processor.getClass());
                                JniComponent.JniComponentBuilder builder = JniComponent.JniComponentBuilder.create(processor.getClass().getCanonicalName()).addProperties(descriptors).addDescription(description).addRelationships(processor.getRelationships());
                                if (dynProperty != null) {
                                    builder.setDynamicProperties();
                                }
                                if (dynRelationShip != null) {
                                    builder.setDynamicRelationships();
                                }
                                components.add(builder.build());
                            }
                        }
                }
                }catch(Throwable e){
                    logger.info("Ignoring ",e);
                }


            }

        serviceLoader = ServiceLoader.load(ControllerService.class, bundle.getClassLoader());
        sli = serviceLoader.iterator();
        while(sli.hasNext()){
            try {
                Object o = sli.next();
                // create a cache of temp ConfigurableComponent instances, the initialize here has to happen before the checks below
                if (o instanceof ConfigurableComponent) {

                    final ConfigurableComponent configurableComponent = (ConfigurableComponent) o;
                    initializeTempComponent(configurableComponent);
                    if (configurableComponent instanceof ControllerService) {
                        final ControllerService cs = ControllerService.class.cast(configurableComponent);
                        if (cs != null) {
                            List<PropertyDescriptor> descriptors = cs.getPropertyDescriptors();
                            final String description = getDescription(cs.getClass());
                            classes.put(cs.getClass().getCanonicalName(),cs.getClass());
                            JniComponent.JniComponentBuilder builder = JniComponent.JniComponentBuilder.create(cs.getClass().getCanonicalName()).addProperties(descriptors).addDescription(description).setIsControllerService();
                            builder.setDynamicProperties();
                            components.add(builder.build());
                        }

                    }

                }
            }catch(Throwable e){
                logger.info("Ignoring ",e);
            }


        }
            return components;
    }
    /**
     * Gets the description from the specified class.
     */
    private static String getDescription(final Class<?> cls) {
        final CapabilityDescription capabilityDesc = cls.getAnnotation(CapabilityDescription.class);
        return capabilityDesc == null ? "" : capabilityDesc.value();
    }

    private static DynamicProperty getDynamicPropertyAnnotation(final Class<?> cls) {
        final DynamicProperty dynProperty = cls.getAnnotation(DynamicProperty.class);
        return dynProperty;
    }

    private static DynamicRelationship getDynamicRelationshipAnnotation(final Class<?> cls) {
        final DynamicRelationship dynamicRelationship = cls.getAnnotation(DynamicRelationship.class);
        return dynamicRelationship;
    }

    private static void initializeTempComponent(final ConfigurableComponent configurableComponent) {
        ExtensionManager manager = new StandardExtensionDiscoveringManager();
        ConfigurableComponentInitializer initializer = null;
        try {
            initializer = ConfigurableComponentInitializerFactory.createComponentInitializer(manager,configurableComponent.getClass());
            initializer.initialize(configurableComponent);
        } catch (final InitializationException e) {
            logger.warn(String.format("Unable to initialize component %s due to %s", configurableComponent.getClass().getName(), e.getMessage()));
        }
    }

    public static List<Method> getAnnotatedMethods(final Class<?> type, final Class<? extends Annotation> annotation) {
        final List<Method> methods = new ArrayList<Method>();
        Class<?> klass = type;
        while (klass != Object.class) {
            for (final Method method : klass.getDeclaredMethods()) {
                if (method.isAnnotationPresent(annotation)) {
                    methods.add(method);
                }
            }
            if (methods.isEmpty())
                klass = klass.getSuperclass();
            else
                break;
        }
        return methods;
    }


    public String getMethod(final String className, final String annotation){
        Method mthd = onScheduledMethod.get(new AbstractMap.SimpleImmutableEntry<>(className,annotation));

        if (mthd == null)
        {
            return null;
        }

        return mthd.getName();
    }

    public String getSignature(final String className, final String annotation){
        Method mthd = onScheduledMethod.get(new AbstractMap.SimpleImmutableEntry<>(className,annotation));
        if (mthd == null)
        {
            return null;
        }
        String ret = "", argTypes="";
        if (mthd.getReturnType().equals(Void.TYPE)){
            ret = "V";
        }
        else{
            ret = classToType(mthd.getReturnType());
        }
        argTypes = "(";
        for(Class<?> type : mthd.getParameterTypes()){
            argTypes += classToType(type);
        }

        argTypes += ")";

        return argTypes + ret;
    }

    private static String classToType(Class<?> type){
        if (type.equals(Integer.TYPE)){
            return "I";
        } else if (type.equals(Boolean.TYPE)) {
            return "Z";
        }
        else if (type.equals(Byte.TYPE)) {
            return "B";
        }
        else if (type.equals(Character.TYPE)) {
            return "C";
        }
        else if (type.equals(Short.TYPE)) {
            return "S";
        }
        else if (type.equals(Long.TYPE)) {
            return "J";
        }
        else if (type.equals(Boolean.TYPE)) {
            return "Z";
        }
        else{
            return "L" + type.getCanonicalName().replace(".","/") + ";";
        }


    }

    public Object createObject(final String className) throws IllegalAccessException, InstantiationException, ClassNotFoundException {
        synchronized (this) {
            Class clazz = classes.get(className);
            if (clazz == null) {
                clazz = parent.loadClass(className);
            }

            if (clazz == null) {
                logger.warn("Could not find {}", className);
                return null;
            } else {
                List<Method> methods = getAnnotatedMethods(clazz, OnScheduled.class);
                methods.stream().forEach(mthd -> onScheduledMethod.put(new AbstractMap.SimpleImmutableEntry<>(className, "OnScheduled"), mthd));

                methods = getAnnotatedMethods(clazz, OnEnabled.class);
                methods.stream().forEach(mthd -> onScheduledMethod.put(new AbstractMap.SimpleImmutableEntry<>(className, "OnEnabled"), mthd));

                methods = getAnnotatedMethods(clazz, OnDisabled.class);
                methods.stream().forEach(mthd -> onScheduledMethod.put(new AbstractMap.SimpleImmutableEntry<>(className, "OnDisabled"), mthd));
            }

            return clazz.newInstance();
        }
    }

}
