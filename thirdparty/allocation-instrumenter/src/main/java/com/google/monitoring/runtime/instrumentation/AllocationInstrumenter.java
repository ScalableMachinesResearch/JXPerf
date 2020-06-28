/*
 * Copyright (C) 2007 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.monitoring.runtime.instrumentation;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.Instrumentation;
import java.lang.instrument.UnmodifiableClassException;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;

import sun.misc.Unsafe;
import java.lang.reflect.Field;
import java.util.Collections;


import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.MemoryPoolMXBean;
import javax.management.NotificationEmitter;
import javax.management.NotificationListener;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.ListenerNotFoundException;
import com.sun.management.GarbageCollectionNotificationInfo;
import javax.management.openmbean.CompositeData;
import java.lang.management.ManagementFactory;
import com.sun.management.GcInfo;
//import com.sun.management.GarbageCollectorMXBean;
import javax.management.NotificationBroadcaster;
import javax.management.MBeanNotificationInfo;
//import sun.management.GarbageCollectorImpl;
//import sun.management.ManagementFactoryHelper;

/**
 * Instruments bytecodes that allocate heap memory to call a recording hook. This will add a static
 * invocation to a recorder function to any bytecode that looks like it will be allocating heap
 * memory allowing users to implement heap profiling schemes.
 */
public class AllocationInstrumenter implements ClassFileTransformer {
  private static final Logger logger = Logger.getLogger(AllocationInstrumenter.class.getName());

  // We can rewrite classes loaded by the bootstrap class loader
  // iff the agent is loaded by the bootstrap class loader.  It is
  // always *supposed* to be loaded by the bootstrap class loader, but
  // this relies on the Boot-Class-Path attribute in the JAR file always being
  // set to the name of the JAR file that contains this agent, which we cannot
  // guarantee programmatically.
  private static volatile boolean canRewriteBootstrap;

  static boolean canRewriteClass(String className, ClassLoader loader) {
    // There are two conditions under which we don't rewrite:
    //  1. If className was loaded by the bootstrap class loader and
    //  the agent wasn't (in which case the class being rewritten
    //  won't be able to call agent methods).
    //  2. If it is java.lang.ThreadLocal, which can't be rewritten because the
    //  JVM depends on its structure.
    if (((loader == null) && !canRewriteBootstrap)
        || className.startsWith("java/lang/ThreadLocal")) {
      return false;
    }
    // third_party/java/webwork/*/ognl.jar contains bad class files.  Ugh.
    if (className.startsWith("ognl/")) {
      return false;
    }

    return true;
  }

  // No instantiating me except in premain() or in {@link JarClassTransformer}.
  AllocationInstrumenter() {}

  
  static final Unsafe unsafe = getUnsafe();
  static final boolean is64bit = true;
  static boolean flag = true;

  public static String printAddresses(Object... objects) {
        String tmp = "";
        //System.out.print(label + ": 0x");
        long last = 0;
        int offset = unsafe.arrayBaseOffset(objects.getClass());
        int scale = unsafe.arrayIndexScale(objects.getClass());
        switch (scale) {
            case 4:
                long factor = is64bit ? 8 : 1;
                final long i1 = (unsafe.getInt(objects, offset) & 0xFFFFFFFFL) * factor;
                tmp = Long.toHexString(i1);
                //System.out.println(Long.toHexString(i1));
                last = i1;
                for (int i = 1; i < objects.length; i++) {
                    final long i2 = (unsafe.getInt(objects, offset + i * 4) & 0xFFFFFFFFL) * factor;
                    if (i2 > last) {
                        //System.out.print(", +" + Long.toHexString(i2 - last));
                        tmp = tmp.concat(", +" + Long.toHexString(i2 - last));
                    }
                    else {
                        //System.out.print(", -" + Long.toHexString(last - i2));
                        tmp = tmp.concat(", -" + Long.toHexString(last - i2));
                    }
                    last = i2;
                }
                break;
            case 8:
                throw new AssertionError("Not supported");
        }
        return tmp;
  }

  private static Unsafe getUnsafe() {
        try {
            Field theUnsafe = Unsafe.class.getDeclaredField("theUnsafe");
            theUnsafe.setAccessible(true);
            return (Unsafe) theUnsafe.get(null);
        } catch (Exception e) {
            throw new AssertionError(e);
        }  
  }

  public native void dataCentric(String addr, long size);
  public native void clearTree();
  public native void removeReclaimedObjectInSplayTree(String addr);
  
  @Override
  protected void finalize() throws Throwable {
      AllocationInstrumenter ai = new  AllocationInstrumenter();
      String res = printAddresses(this);
      ai.removeReclaimedObjectInSplayTree(res);
      super.finalize();
  }

  public static void register_callback(String[] args)
  {
    AllocationInstrumenter ai = new  AllocationInstrumenter();

    AllocationRecorder.addSampler(new Sampler() {
        
        public void sampleAllocation(int count, String desc, Object newObj, long size) {
            if (flag == true) {
              List<GarbageCollectorMXBean> gcbeans = java.lang.management.ManagementFactory.getGarbageCollectorMXBeans();
              for (GarbageCollectorMXBean gcbean : gcbeans) {
                NotificationEmitter emitter = (NotificationEmitter) gcbean;
                //System.out.println(gcbean.getName());

                NotificationListener listener = (notification, handback) -> {
                  if (notification.getType().equals(GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION)) {
                      ai.clearTree();            
                  }
                };
                emitter.addNotificationListener(listener, null, null);
              }
              flag = false;
            }
            if(size > 1024)
            {
                try
                {
                    String res = printAddresses(newObj);
                    ai.dataCentric(res, size);
                } catch(Exception e) {
                    return;
                }
            }
        }
    });
  }


public static void agentmain(String agentArgs, Instrumentation inst) {

	System.load(System.getenv("JXPerf_HOME") + "/build/libagent.so");

    AllocationRecorder.setInstrumentation(inst);

    // Force eager class loading here.  The instrumenter relies on these classes.  If we load them
    // for the first time during instrumentation, the instrumenter will try to rewrite them.  But
    // the instrumenter needs these classes to run, so it will try to load them during that rewrite
    // pass.  This results in a ClassCircularityError.
    try {
      Class.forName("sun.security.provider.PolicyFile");
      Class.forName("java.util.ResourceBundle");
      Class.forName("java.util.Date");
    } catch (Throwable t) {
      // NOP
    }

    if (!inst.isRetransformClassesSupported()) {
      System.err.println("Some JDK classes are already loaded and will not be instrumented.");
    }

    // Don't try to rewrite classes loaded by the bootstrap class
    // loader if this class wasn't loaded by the bootstrap class
    // loader.
    if (AllocationRecorder.class.getClassLoader() != null) {
      canRewriteBootstrap = false;
      // The loggers aren't installed yet, so we use println.
      System.err.println("Class loading breakage: Will not be able to instrument JDK classes");
      return;
    }

    canRewriteBootstrap = true;
    List<String> args = Arrays.asList(agentArgs == null ? new String[0] : agentArgs.split(","));

    // When "subclassesAlso" is specified, samplers are also invoked when
    // SubclassOfA.<init> is called while only class A is specified to be
    // instrumented.
    ConstructorInstrumenter.subclassesAlso = args.contains("subclassesAlso");
    inst.addTransformer(new ConstructorInstrumenter(), inst.isRetransformClassesSupported());

    if (!args.contains("manualOnly")) {
      bootstrap(inst);
    }
  }



  public static void premain(String agentArgs, Instrumentation inst) {

	System.load(System.getenv("JXPerf_HOME") + "/build/libagent.so");

    AllocationRecorder.setInstrumentation(inst);

    // Force eager class loading here.  The instrumenter relies on these classes.  If we load them
    // for the first time during instrumentation, the instrumenter will try to rewrite them.  But
    // the instrumenter needs these classes to run, so it will try to load them during that rewrite
    // pass.  This results in a ClassCircularityError.
    try {
      Class.forName("sun.security.provider.PolicyFile");
      Class.forName("java.util.ResourceBundle");
      Class.forName("java.util.Date");
    } catch (Throwable t) {
      // NOP
    }

    if (!inst.isRetransformClassesSupported()) {
      System.err.println("Some JDK classes are already loaded and will not be instrumented.");
    }

    // Don't try to rewrite classes loaded by the bootstrap class
    // loader if this class wasn't loaded by the bootstrap class
    // loader.
    if (AllocationRecorder.class.getClassLoader() != null) {
      canRewriteBootstrap = false;
      // The loggers aren't installed yet, so we use println.
      System.err.println("Class loading breakage: Will not be able to instrument JDK classes");
      return;
    }

    canRewriteBootstrap = true;
    List<String> args = Arrays.asList(agentArgs == null ? new String[0] : agentArgs.split(","));

    // When "subclassesAlso" is specified, samplers are also invoked when
    // SubclassOfA.<init> is called while only class A is specified to be
    // instrumented.
    ConstructorInstrumenter.subclassesAlso = args.contains("subclassesAlso");
    inst.addTransformer(new ConstructorInstrumenter(), inst.isRetransformClassesSupported());

    if (!args.contains("manualOnly")) {
      bootstrap(inst);
    }
  }

  private static void bootstrap(Instrumentation inst) {
    inst.addTransformer(new AllocationInstrumenter(), inst.isRetransformClassesSupported());

    if (!canRewriteBootstrap) {
      return;
    }

    /*
     * Transform a single class first.
     *
     * The problem we need to avoid is this:
     *
     * - Our main retransformClasses() call transforms all classes that were loaded *before* that
     * retransformClasses() call.
     *
     * - Our addTransformer() call transforms "all future class definitions... except definitions of
     * classes upon which any registered transformer is dependent." (See
     * Instrumentation.addTransformer.)
     *
     * Thus, there's a window of vulnerability: We miss anything that our transformers load during
     * their transformation after not having loaded it during their construction.
     *
     * To avoid this, we need to make sure that our main retransformClasses() call happens after our
     * transformers have loaded everything that they depend on. Our best shot at this is to run a
     * transformation on a single class and only *then* request the list of already loaded classes.
     */
    try {
      inst.retransformClasses(new Class<?>[] {Object.class});
    } catch (UnmodifiableClassException e) {
      System.err.println("AllocationInstrumenter was unable to retransform java.lang.Object.");
    }

    // Get the set of already loaded classes that can be rewritten.
    Class<?>[] classes = inst.getAllLoadedClasses();
    ArrayList<Class<?>> classList = new ArrayList<Class<?>>();
    for (int i = 0; i < classes.length; i++) {
      if (inst.isModifiableClass(classes[i]) && classes[i] != Object.class) {
        classList.add(classes[i]);
      }
    }

    // Reload classes, if possible.
    Class<?>[] workaround = new Class<?>[classList.size()];
    try {
      inst.retransformClasses(classList.toArray(workaround));
    } catch (UnmodifiableClassException e) {
      System.err.println("AllocationInstrumenter was unable to retransform early loaded classes.");
    }
  }

  @Override
  public byte[] transform(
      ClassLoader loader,
      String className,
      Class<?> classBeingRedefined,
      ProtectionDomain protectionDomain,
      byte[] origBytes) {
    if (!canRewriteClass(className, loader)) {
      return null;
    }

    return instrument(origBytes, loader);
  }

  /**
   * Given the bytes representing a class, go through all the bytecode in it and instrument any
   * occurrences of new/newarray/anewarray/multianewarray with pre- and post-allocation hooks. Even
   * more fun, intercept calls to the reflection API's Array.newInstance() and instrument those too.
   *
   * @param originalBytes the original <code>byte[]</code> code.
   * @param recorderClass the <code>String</code> internal name of the class containing the recorder
   *     method to run.
   * @param recorderMethod the <code>String</code> name of the recorder method to run.
   * @param loader the <code>ClassLoader</code> for this class.
   * @return the instrumented <code>byte[]</code> code.
   */
  public static byte[] instrument(
      byte[] originalBytes, String recorderClass, String recorderMethod, ClassLoader loader) {
    try {
      ClassReader cr = new ClassReader(originalBytes);
      // The verifier in JDK7+ requires accurate stackmaps, so we use
      // COMPUTE_FRAMES.
      ClassWriter cw = new StaticClassWriter(cr, ClassWriter.COMPUTE_FRAMES, loader);

      VerifyingClassAdapter vcw = new VerifyingClassAdapter(cw, originalBytes, cr.getClassName());
      ClassVisitor adapter = new AllocationClassAdapter(vcw, recorderClass, recorderMethod);

      cr.accept(adapter, ClassReader.SKIP_FRAMES);

      return vcw.toByteArray();
    } catch (RuntimeException e) {
      logger.log(Level.WARNING, "Failed to instrument class.", e);
      throw e;
    } catch (Error e) {
      logger.log(Level.WARNING, "Failed to instrument class.", e);
      throw e;
    }
  }

  /**
   * @see #instrument(byte[], String, String, ClassLoader) documentation for the 4-arg version. This
   *     is a convenience version that uses the recorder in this class.
   * @param originalBytes The original version of the class.
   * @param loader The ClassLoader of this class.
   * @return the instrumented version of this class.
   */
  public static byte[] instrument(byte[] originalBytes, ClassLoader loader) {
    return instrument(
        originalBytes,
        "com/google/monitoring/runtime/instrumentation/AllocationRecorder",
        "recordAllocation",
        loader);
  }
}
