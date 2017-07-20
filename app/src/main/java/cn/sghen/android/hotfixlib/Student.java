package cn.sghen.android.hotfixlib;

/**
 * Created by lgb on 17-7-20.
 */

public class Student {

    private long id;
    private String name;
    private boolean isMale;
    private int age;

    public Student(long id, String name, boolean isMale, int age) {
        this.id = id;
        this.name = name;
        this.isMale = isMale;
        this.age = age;
    }

    @Override
    public String toString() {
        return "Student{" +
                "id=" + id +
                ", name='" + name + '\'' +
                ", isMale=" + isMale +
                ", age=" + age +
                '}';
    }
}
