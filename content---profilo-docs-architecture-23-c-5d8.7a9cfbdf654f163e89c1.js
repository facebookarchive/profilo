(window.webpackJsonp=window.webpackJsonp||[]).push([[4],{44:function(e,t,n){"use strict";n.r(t),n.d(t,"frontMatter",(function(){return o})),n.d(t,"rightToc",(function(){return l})),n.d(t,"default",(function(){return s}));n(0);var r=n(60);function a(){return(a=Object.assign||function(e){for(var t=1;t<arguments.length;t++){var n=arguments[t];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(e[r]=n[r])}return e}).apply(this,arguments)}function i(e,t){if(null==e)return{};var n,r,a=function(e,t){if(null==e)return{};var n,r,a={},i=Object.keys(e);for(r=0;r<i.length;r++)n=i[r],t.indexOf(n)>=0||(a[n]=e[n]);return a}(e,t);if(Object.getOwnPropertySymbols){var i=Object.getOwnPropertySymbols(e);for(r=0;r<i.length;r++)n=i[r],t.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(e,n)&&(a[n]=e[n])}return a}var o={id:"architecture",title:"Architecture",sidebar_label:"Architecture"},l=[{value:"Glossary",id:"glossary",children:[]},{value:"Fundamental design concerns",id:"fundamental-design-concerns",children:[{value:"Trace Providers",id:"trace-providers",children:[]}]},{value:"Trace configuration",id:"trace-configuration",children:[]},{value:"Adding events to the trace",id:"adding-events-to-the-trace",children:[]}],c={rightToc:l};function s(e){var t=e.components,n=i(e,["components"]);return Object(r.b)("wrapper",a({},c,n,{components:t,mdxType:"MDXLayout"}),Object(r.b)("h2",{id:"glossary"},"Glossary"),Object(r.b)("ul",null,Object(r.b)("li",{parentName:"ul"},Object(r.b)("strong",{parentName:"li"},"Trace"),": a file containing a time window of performance data from an application."),Object(r.b)("li",{parentName:"ul"},Object(r.b)("strong",{parentName:"li"},"Trigger"),": an attempt to start a trace."),Object(r.b)("li",{parentName:"ul"},Object(r.b)("strong",{parentName:"li"},"TraceController"),": an object controlling whether Profilo should actually start a trace. Each Trigger is associated with one TraceController."),Object(r.b)("li",{parentName:"ul"},Object(r.b)("strong",{parentName:"li"},"TraceProvider"),": an object providing a subset of data in the trace. Examples are stack traces, systrace (atrace), system counters, etc. Different providers can be enabled in different traces, all at the discretion of the TraceController."),Object(r.b)("li",{parentName:"ul"},Object(r.b)("strong",{parentName:"li"},"ConfigProvider"),": an object providing a Config implementation, which ",Object(r.b)("strong",{parentName:"li"},"TraceController")," can check to determine whether to satisfy the request.")),Object(r.b)("h2",{id:"fundamental-design-concerns"},"Fundamental design concerns"),Object(r.b)("p",null,"There are a few concerns that drive most of Profilo's design."),Object(r.b)("ol",null,Object(r.b)("li",{parentName:"ol"},"The ability to configure separate layers of data (providers) inside the collected traces."),Object(r.b)("li",{parentName:"ol"},'The ability to decouple "trace requests" from "trace configuration".'),Object(r.b)("li",{parentName:"ol"},"The ability to efficiently write data into the trace file.")),Object(r.b)("p",null,"Let's go through each in turn to investigate how it was handled."),Object(r.b)("h3",{id:"trace-providers"},"Trace Providers"),Object(r.b)("p",null,"Trace providers are fundamentally just bits in a bitset. The bitset is managed by ",Object(r.b)("inlineCode",{parentName:"p"},"TraceEvents")," while the bits are assigned by ",Object(r.b)("inlineCode",{parentName:"p"},"ProvidersRegistry"),". Registering a provider requires that you associate it with a name."),Object(r.b)("p",null,"This name may appear in a config (see below) and the numeric constant (single bit in the bitset) may be used directly to check if the provider is enabled in the trace."),Object(r.b)("p",null,"The ",Object(r.b)("inlineCode",{parentName:"p"},"TraceProvider")," interface (and ",Object(r.b)("inlineCode",{parentName:"p"},"BaseTraceProvider")," abstract class) will receive notification at the beginning and end of a trace to perform setup and teardown work."),Object(r.b)("p",null,"It's easy to get the interactions between registering and using providers wrong, so we recommend this pattern for your custom data needs:"),Object(r.b)("pre",null,Object(r.b)("code",a({parentName:"pre"},{className:"language-java"}),'public final class MyProvider extends BaseTraceProvider {\n  public static final int PROVIDER_MY_PROVIDER =\n      ProvidersRegistry.newProvider("my_provider");\n\n  @Override\n  public void enable() {}\n\n  @Override\n  public void disable() {}\n\n  @Override\n  protected int getSupportedProviders() {\n    return PROVIDER_MY_PROVIDER;\n  }\n\n  @Override\n  protected int getTracingProviders() {\n    return PROVIDER_MY_PROVIDER;\n  }\n}\n')),Object(r.b)("h2",{id:"trace-configuration"},"Trace configuration"),Object(r.b)("p",null,Object(r.b)("strong",{parentName:"p"},"TraceControllers")," can use external configuration to determine whether to allow a trace request."),Object(r.b)("p",null,"An example may be a hypothetical trigger ",Object(r.b)("inlineCode",{parentName:"p"},"STRING_TRIGGER")," with the following associated controller:"),Object(r.b)("pre",null,Object(r.b)("code",a({parentName:"pre"},{className:"language-java"}),'public class StringController implements TraceController {\n\n  @Override\n  public int evaluateConfig(@Nullable Object context, ControllerConfig config) {\n    // Downcast to the types we expect.\n    String str = (String) context;\n    StringControllerConfig strconfig = (StringControllerConfig) config;\n\n    return strconfig.hasKey(str);\n  }\n\n  @Override\n  public TraceContext.TraceConfigExtras getTraceConfigExtras(\n      long longContext, @Nullable Object context, ControllerConfig config) {\n    StringControllerConfig strconfig = (StringControllerConfig) config;\n\n    TreeMap<String, int[]> extraIntArrMap = new TreeMap<>();\n    extraIntArrMap.put("sampling_rate_ms", strconfig.get(str).cpuSamplingRateMs);\n    return new TraceContext.TraceConfigExtras(extraIntMap, null, null);\n  }\n\n  @Override\n  public boolean contextsEqual(int fstInt, @Nullable Object fst, int sndInt, @Nullable Object snd) {\n    if ((fst == null && snd != null) ||\n      (fst != null && snd == null)) {\n        return false;\n    }\n    return fst == snd || fst.equals(snd);\n  }\n\n  public boolean isConfigurable() {\n    return true;\n  }\n')),Object(r.b)("p",null,"This controller can then be used as follows:"),Object(r.b)("pre",null,Object(r.b)("code",a({parentName:"pre"},{className:"language-java"}),'TraceControl.get().startTrace(\n  STRING_TRIGGER,\n  0, // flags\n  0, // intContext\n  "my_unique_string_identifying_this_trace_request");\n')),Object(r.b)("p",null,"It is the responsibility of your custom ",Object(r.b)("inlineCode",{parentName:"p"},"ConfigProvider")," to return an instance of ",Object(r.b)("inlineCode",{parentName:"p"},"StringControllerConfig")," when asked for the controller config for ",Object(r.b)("inlineCode",{parentName:"p"},"STRING_TRIGGER"),"."),Object(r.b)("p",null,"You can set your ",Object(r.b)("inlineCode",{parentName:"p"},"ConfigProvider")," at any point, Profilo takes care to ensure that the config presented to ",Object(r.b)("inlineCode",{parentName:"p"},"TraceControllers")," is consistent while a trace is active."),Object(r.b)("h2",{id:"adding-events-to-the-trace"},"Adding events to the trace"),Object(r.b)("p",null,Object(r.b)("strong",{parentName:"p"},"NOTE:")," Subject to imminent change."),Object(r.b)("p",null,"The current APIs to log data into the trace are ",Object(r.b)("inlineCode",{parentName:"p"},"com.facebook.profilo.logger.Logger")," (Java) and ",Object(r.b)("inlineCode",{parentName:"p"},"facebook::profilo::logger::Logger")," (C++)."),Object(r.b)("p",null,"They're not very extensible but we're working on fixing the APIs and data formats. Until then, you can look at how the builtin providers use them."))}s.isMDXComponent=!0}}]);