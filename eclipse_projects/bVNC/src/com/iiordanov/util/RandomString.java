package com.iiordanov.util;

import java.util.Random;

public class RandomString
{
	private Random r;

	public RandomString() {
		r = new Random();
	}

	public String randomString(final int length, int maxValue) {
		StringBuilder sb = new StringBuilder();
		for(int i = 0; i < length; i++) {
			char c = (char)(r.nextInt((int)(Character.MAX_VALUE)));
			sb.append(c);
		}
		return sb.toString();
	}

	public String randomString(final int length) {
		return randomString(length, Character.MAX_VALUE);
	}

	public String randomLowerCaseString(final int length) {
		StringBuilder sb = new StringBuilder();
		for(int i = 0; i < length; i++) {
			char c = (char)(r.nextInt((int)(25)) + 97); // Get only a-z
			sb.append(c);
		}
		return sb.toString();
	}
}

